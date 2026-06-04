#pragma once

#include "solver_interface.hpp"
#include "utils/estimation.hpp"
#include <queue>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cassert>
#include <cstdint>

namespace tau {

// =============================================================================
// DecMCTSSolverV3
// =============================================================================
// Sobre v2, añade dos cambios estructurales tomados de Pérez-Hurtado et al.
// (MO-ISMCTS aplicado a MRTA con incertidumbre):
//
// 1) Estadística doble: cada nodo guarda N_disc (visit count descontado) y
//    N_avail_disc (availability count descontado, cuántas veces estaba en
//    availableActions cuando se visitó al padre). La fórmula UCB pasa a usar
//    log(N_avail_disc) / N_disc en lugar de log(parent_N_disc) / N_disc.
//    Esto corrige el sesgo cuando la factibilidad varía rollout a rollout
//    (típico en MRTAU por batería/ventanas temporales).
//
// 2) Chance nodes tras EXECUTE_TASK: el árbol pasa a ser bipartito. Tras un
//    nodo ACTION cuya acción es EXECUTE_TASK, se inserta un nodo CHANCE con
//    rama SUCCESS y rama FAIL. Esto evita el strategy fusion: el árbol ya no
//    promedia rollouts incompatibles (planes post-éxito vs planes post-fallo).
//    Los outcomes los dicta el simulador, no UCB.
//
// El resto de mejoras de v2 se mantienen:
//   - D-UCT con descuento perezoso (gamma configurable)
//   - doble rollout opcional para f^r (useDiffReward configurable)
//   - política de rollout marginal-aware
//   - consolidateTimestamps tras re-enraizar
//
// Bundles comunicados: planos (vector<TaskID>) como en v2. Al extraer la
// distribución se sigue el camino más visitado a través de los chance nodes
// (los chance nodes no aparecen en el bundle plano, son una estructura interna
// del árbol).
// =============================================================================

class DecMCTSSolverV3 : public ISolver {

    // =========================================================================
    // Constantes de configuración
    // =========================================================================
    static constexpr double C_EXPLORE           = 0.7;
    static constexpr int    ITERATIONS_PER_CALL  = 30;
    static constexpr int    EMERGENCY_ITERS      = 300;
    static constexpr int    MAX_ROLLOUT_EVENTS   = 200000;

    // Outcomes discretos para chance nodes
    static constexpr int OUTCOME_FAIL    = 0;
    static constexpr int OUTCOME_SUCCESS = 1;

    // =========================================================================
    // Nodo MCTS (ahora puede ser ACTION o CHANCE)
    // =========================================================================
    struct MCTSNode {
        enum class NodeType : uint8_t { ACTION, CHANCE };

        NodeType nodeType;
        Action   action;       // válido si ACTION (la raíz usa Type::START como dummy)
        int      outcome;      // válido si CHANCE (OUTCOME_SUCCESS / OUTCOME_FAIL)
        MCTSNode* parent;
        std::vector<std::unique_ptr<MCTSNode>> children;

        // Estadísticas descontadas
        double N_disc       = 0.0;  // visit count descontado
        double N_avail_disc = 0.0;  // availability count descontado (solo significativo en ACTION)
        double Q_disc       = 0.0;
        int    lastTick     = 0;

        // Constructores por fábrica
        static std::unique_ptr<MCTSNode> makeAction(Action a, MCTSNode* p) {
            auto n = std::unique_ptr<MCTSNode>(new MCTSNode());
            n->nodeType = NodeType::ACTION;
            n->action   = a;
            n->outcome  = 0;
            n->parent   = p;
            return n;
        }
        static std::unique_ptr<MCTSNode> makeChance(int outcome, MCTSNode* p) {
            auto n = std::unique_ptr<MCTSNode>(new MCTSNode());
            n->nodeType = NodeType::CHANCE;
            n->action   = Action(Action::Type::START); // dummy
            n->outcome  = outcome;
            n->parent   = p;
            return n;
        }

        // Búsqueda de hijos por tipo
        MCTSNode* findActionChild(Action a) const {
            for (const auto& c : children) {
                if (c->nodeType == NodeType::ACTION &&
                    c->action.getType()   == a.getType() &&
                    c->action.getTarget() == a.getTarget())
                    return c.get();
            }
            return nullptr;
        }
        MCTSNode* findChanceChild(int outcome) const {
            for (const auto& c : children) {
                if (c->nodeType == NodeType::CHANCE && c->outcome == outcome)
                    return c.get();
            }
            return nullptr;
        }

        // Aplica gamma^(currentTick - lastTick) perezosamente a N_disc, N_avail_disc y Q_disc.
        // Tabla cacheada por valor de gamma (igual que v2).
        void decayTo(int currentTick, double gamma) {
            if (currentTick <= lastTick) return;
            int delta = currentTick - lastTick;

            static double cachedGamma = -1.0;
            static std::vector<double> table;
            if (gamma != cachedGamma) {
                cachedGamma = gamma;
                table.clear();
                double v = 1.0;
                while (v > 1e-15 && table.size() < 40000) {
                    table.push_back(v);
                    v *= gamma;
                }
            }

            double factor = (delta < static_cast<int>(table.size())) ? table[delta] : 0.0;
            if (factor < 1e-15) {
                N_disc = 0.0; N_avail_disc = 0.0; Q_disc = 0.0;
            } else {
                N_disc       *= factor;
                N_avail_disc *= factor;
                Q_disc       *= factor;
            }
            lastTick = currentTick;
        }

        // Hijo ACTION con mayor N_disc (política de explotación final)
        MCTSNode* mostVisitedActionChild(int currentTick, double gamma) {
            MCTSNode* best = nullptr;
            double bestN = -1.0;
            for (auto& c : children) {
                if (c->nodeType != NodeType::ACTION) continue;
                c->decayTo(currentTick, gamma);
                if (c->N_disc > bestN) { bestN = c->N_disc; best = c.get(); }
            }
            return best;
        }

        // Chance child con mayor N_disc (para extractDistribution)
        MCTSNode* mostVisitedChanceChild(int currentTick, double gamma) {
            MCTSNode* best = nullptr;
            double bestN = -1.0;
            for (auto& c : children) {
                if (c->nodeType != NodeType::CHANCE) continue;
                c->decayTo(currentTick, gamma);
                if (c->N_disc > bestN) { bestN = c->N_disc; best = c.get(); }
            }
            return best;
        }

        // Aplica el descuento pendiente a todo el subárbol y fija lastTick al actual
        // (H3 fix de v2: tras re-enraizar, evitar que D-UCT borre la información de golpe).
        static void consolidateTimestamps(MCTSNode* node, int currentTick, double gamma) {
            if (!node) return;
            node->decayTo(currentTick, gamma);
            for (auto& child : node->children)
                consolidateTimestamps(child.get(), currentTick, gamma);
        }

    private:
        MCTSNode() : nodeType(NodeType::ACTION),
                     action(Action::Type::START),
                     outcome(0), parent(nullptr) {}
    };

    // =========================================================================
    // Selección D-UCT entre hijos ACTION
    // =========================================================================
    // Filtra por hijos cuya acción está en `avail` e incrementa N_avail_disc
    // de cada uno (visita al padre cuenta como "disponible"). Devuelve el primer
    // hijo con N_disc≈0 (equivalente a UCB=∞), o el de mayor
    //   U = Q/N + 2·Cp·sqrt(log(N_avail_disc) / N_disc).
    //
    // Devuelve nullptr si no hay ningún hijo ACTION cuya acción esté en avail
    // (situación típica cuando todas las acciones disponibles requieren expansión).
    MCTSNode* selectActionChildDUCT(MCTSNode* parent, const std::vector<Action>& avail) {
        std::set<std::pair<int, int>> availSet;
        for (const auto& a : avail) {
            availSet.emplace(static_cast<int>(a.getType()),
                             static_cast<int>(a.getTarget()));
        }

        // Descontar e incrementar N_avail_disc de hijos disponibles
        for (auto& c : parent->children) {
            if (c->nodeType != MCTSNode::NodeType::ACTION) continue;
            c->decayTo(globalTick, gamma_);
            const auto key = std::make_pair(static_cast<int>(c->action.getType()),
                                            static_cast<int>(c->action.getTarget()));
            if (availSet.count(key)) c->N_avail_disc += 1.0;
        }

        // Si algún hijo disponible es "fresco" (N_disc ≈ 0), devolverlo
        for (auto& c : parent->children) {
            if (c->nodeType != MCTSNode::NodeType::ACTION) continue;
            const auto key = std::make_pair(static_cast<int>(c->action.getType()),
                                            static_cast<int>(c->action.getTarget()));
            if (!availSet.count(key)) continue;
            if (c->N_disc < 1e-9) return c.get();
        }

        // D-UCT entre disponibles
        MCTSNode* best = nullptr;
        double bestU = -std::numeric_limits<double>::infinity();
        for (auto& c : parent->children) {
            if (c->nodeType != MCTSNode::NodeType::ACTION) continue;
            const auto key = std::make_pair(static_cast<int>(c->action.getType()),
                                            static_cast<int>(c->action.getTarget()));
            if (!availSet.count(key)) continue;
            double availN = std::max(c->N_avail_disc, 1.0);
            double Fbar   = c->Q_disc / c->N_disc;
            double bonus  = 2.0 * C_EXPLORE * std::sqrt(std::log(availN) / c->N_disc);
            double U      = Fbar + bonus;
            if (U > bestU) { bestU = U; best = c.get(); }
        }
        return best;
    }

    // =========================================================================
    // Estado del simulador de rollout
    // =========================================================================
    struct RolloutState {
        std::map<RobotID, Robot> robots;
        std::map<TaskID,  Task>  tasks;
        Time globalTime = 0.0;

        bool isFinal() const {
            for (const auto& [id, r] : robots)
                if (r.status != RobotStatus::FAILED && r.status != RobotStatus::FINISHED)
                    return false;
            return true;
        }
    };

    enum class REType : int {
        TASK_END        = 0,
        TASK_START      = 1,
        TASK_EXPIRATION = 2,
        ROBOT_DECISION  = 3
    };

    struct RolloutEvent {
        Time   time;
        REType type;
        RobotID robotID = NULL_ID;
        TaskID  taskID  = NULL_ID;
        int payload     = 0;
        int tieBreaker  = 0;

        bool operator>(const RolloutEvent& o) const {
            if (time != o.time) return time > o.time;
            if (type != o.type) return static_cast<int>(type) > static_cast<int>(o.type);
            return tieBreaker > o.tieBreaker;
        }
    };

    using EventQueue = std::priority_queue<RolloutEvent,
                                           std::vector<RolloutEvent>,
                                           std::greater<RolloutEvent>>;

    enum class SimMode { ACTIVE_ME_WITH_TREE, PASSIVE_ME };

    // =========================================================================
    // Miembros del solver
    // =========================================================================
    RobotID myId;
    double  gamma_;
    bool    useDiffReward_;
    std::unique_ptr<MCTSNode> root;
    mutable std::mt19937 rng;
    int globalTick = 0;

    // Memoria entre llamadas a decideNextAction para gestionar el avance
    // bietápico del root tras observar el outcome real de la última EXECUTE_TASK.
    bool   lastWasExecuteTask = false;
    TaskID lastExecuteTaskId  = NULL_ID;

    // =========================================================================
    // Utilidades de física (idénticas a v2)
    // =========================================================================
    Time travelTime(RobotID rId, NodeID src, NodeID dst,
                    const std::shared_ptr<const Scenario>& sc) const {
        if (src == dst) return 0.0;
        return sc->distanceBetween(src, dst) / sc->robots.at(rId).navigationVelocity;
    }

    BatteryLevel batteryAfterNav(RobotID rId, BatteryLevel bat,
                                 NodeID src, NodeID dst,
                                 const std::shared_ptr<const Scenario>& sc) const {
        if (src == dst) return bat;
        Time tt = travelTime(rId, src, dst, sc);
        return bat - sc->robots.at(rId).batteryRateWhileNavigating * tt;
    }

    void pushDecision(EventQueue& q, Time t, RobotID id) {
        q.push({t, REType::ROBOT_DECISION, id, NULL_ID, 0,
                std::uniform_int_distribution<int>(0, 10'000'000)(rng)});
    }

    RolloutState makeState(const Observation& obs) const {
        RolloutState s;
        s.globalTime = obs.getCurrentTime();
        s.robots     = obs.getKnownRobots();
        s.robots[obs.getId()] = obs.getMyState();
        s.tasks      = obs.getKnownTasks();
        return s;
    }

    EventQueue makeEventQueue(const RolloutState& s,
                               const std::shared_ptr<const Scenario>& sc) {
        EventQueue q;
        for (const auto& [rId, robot] : s.robots) {
            if (robot.status == RobotStatus::AVAILABLE) {
                Time t = std::max(s.globalTime, robot.time);
                pushDecision(q, t, rId);
            }
        }
        for (const auto& [tId, task] : s.tasks) {
            const TaskInfo& tInfo = sc->getTasks().at(tId);
            switch (task.status) {
                case TaskStatus::PENDING:
                    if (tInfo.latestStart > s.globalTime)
                        q.push({tInfo.latestStart, REType::TASK_EXPIRATION,
                                NULL_ID, tId, 0, 0});
                    break;
                case TaskStatus::ASSIGNED: {
                    Time t = std::max(s.globalTime + 0.001, task.initTime);
                    q.push({t, REType::TASK_START, NULL_ID, tId, 0, 0});
                    break;
                }
                case TaskStatus::EXECUTING: {
                    double p   = tInfo.successProb;
                    Time expDur = p * tInfo.averageSuccessTime
                                + (1.0 - p) * tInfo.averageFailTime;
                    Time endT = std::max(s.globalTime + 0.001, task.initTime + expDur);
                    int payload = (std::uniform_real_distribution<>(0.0, 1.0)(rng) < p) ? 1 : 0;
                    q.push({endT, REType::TASK_END, NULL_ID, tId, payload, 0});
                    break;
                }
                default: break;
            }
        }
        return q;
    }

    // =========================================================================
    // Pre-cómputo de blockingProb (idéntico a v2)
    // =========================================================================
    std::unordered_map<TaskID, double> computeBlockingProb(
        RobotID excludeId,
        const std::map<RobotID, std::vector<TaskID>>& sampledBundles,
        const std::shared_ptr<const Scenario>& sc) const {
        std::unordered_map<TaskID, int> countOthers;
        for (const auto& [rId, bundle] : sampledBundles) {
            if (rId == excludeId) continue;
            std::set<TaskID> seen;
            for (TaskID tId : bundle)
                if (seen.insert(tId).second) countOthers[tId]++;
        }
        std::unordered_map<TaskID, double> result;
        result.reserve(sc->getTasks().size());
        for (const auto& [tId, tInfo] : sc->getTasks()) {
            auto it = countOthers.find(tId);
            int n = (it == countOthers.end()) ? 0 : it->second;
            result[tId] = (n >= tInfo.requiredWorkers) ? tInfo.successProb : 0.0;
        }
        return result;
    }

    // Política de rollout marginal-aware (idéntica a v2)
    Action rolloutAction(RobotID rId, const RolloutState& s,
                          const std::unordered_map<TaskID, double>& blockingProb,
                          const std::shared_ptr<const Scenario>& sc) const {
        const Robot& robot   = s.robots.at(rId);
        const double battCap = sc->robots.at(rId).batteryCapacity;
        bool anyPending = false, anyBatteryUnreachable = false;
        TaskID bestTask  = NULL_ID;
        double bestScore = -std::numeric_limits<double>::infinity();

        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::PENDING) continue;
            anyPending = true;
            const TaskInfo& tInfo = sc->getTasks().at(tId);
            Distance dist = sc->distanceBetween(robot.node, tInfo.node);
            Time tt = dist / sc->robots.at(rId).navigationVelocity;
            BatteryLevel cBat = tt * sc->robots.at(rId).batteryRateWhileNavigating;
            if (robot.time + tt > tInfo.latestStart) continue;
            if (cBat > robot.batteryLevel) { anyBatteryUnreachable = true; continue; }
            double pSelf  = tInfo.successProb;
            auto itBlock  = blockingProb.find(tId);
            double pBlock = (itBlock == blockingProb.end()) ? 0.0 : itBlock->second;
            double value  = pSelf * (1.0 - pBlock);
            double execExp = pSelf * tInfo.averageSuccessTime
                           + (1.0 - pSelf) * tInfo.averageFailTime;
            double cost   = std::max(0.1, tt + execExp);
            double score  = value / cost;
            if (score > bestScore) { bestScore = score; bestTask = tId; }
        }
        if (!anyPending)   return Action(Action::Type::FINISH);
        if (bestTask == NULL_ID) {
            if (anyBatteryUnreachable && robot.batteryLevel < battCap * 0.99)
                return Action(Action::Type::RECHARGE);
            return Action(Action::Type::FINISH);
        }
        return Action(Action::Type::EXECUTE_TASK, bestTask);
    }

    // Acciones disponibles (idéntica a v2)
    std::vector<Action> availableActions(const RolloutState& s,
                                          const std::shared_ptr<const Scenario>& sc) const {
        const Robot& robot   = s.robots.at(myId);
        const double battCap = sc->robots.at(myId).batteryCapacity;
        std::vector<Action> actions;
        bool anyPending = false, anyBatteryUnreachable = false;
        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::PENDING) continue;
            anyPending = true;
            const TaskInfo& tInfo = sc->getTasks().at(tId);
            Distance dist = sc->distanceBetween(robot.node, tInfo.node);
            Time tt = dist / sc->robots.at(myId).navigationVelocity;
            BatteryLevel cost = tt * sc->robots.at(myId).batteryRateWhileNavigating;
            if (robot.time + tt > tInfo.latestStart) continue;
            if (cost > robot.batteryLevel) { anyBatteryUnreachable = true; continue; }
            actions.push_back(Action(Action::Type::EXECUTE_TASK, tId));
        }
        if (!anyPending)    return {Action(Action::Type::FINISH)};
        if (actions.empty()) {
            if (anyBatteryUnreachable && robot.batteryLevel < battCap * 0.99)
                return {Action(Action::Type::RECHARGE)};
            return {Action(Action::Type::FINISH)};
        }
        if (robot.batteryLevel < battCap * 0.99)
            actions.push_back(Action(Action::Type::RECHARGE));
        return actions;
    }

    // Muestreo de bundles vecinos (idéntico a v2)
    std::map<RobotID, std::vector<TaskID>> sampleBundles(
        const std::map<RobotID, Distribution>& dists) {
        std::map<RobotID, std::vector<TaskID>> bundles;
        for (const auto& [rId, dist] : dists) {
            if (rId == myId || dist.empty()) continue;
            double total = 0.0;
            for (const auto& [b, p] : dist) total += p;
            if (total <= 0.0) continue;
            double sample = std::uniform_real_distribution<>(0.0, total)(rng);
            for (const auto& [b, p] : dist) {
                sample -= p;
                if (sample <= 0.0) { bundles[rId] = b; break; }
            }
        }
        return bundles;
    }

    // Decisión de robot vecino (idéntica a v2)
    Action decideOtherRobot(RobotID rId, const RolloutState& s,
                             const std::map<RobotID, std::vector<TaskID>>& sampledBundles,
                             std::map<RobotID, int>& bundlePos,
                             const std::unordered_map<TaskID, double>& blockingProb,
                             const std::shared_ptr<const Scenario>& sc) const {
        auto it = sampledBundles.find(rId);
        if (it != sampledBundles.end()) {
            const auto& bundle = it->second;
            int& pos = bundlePos[rId];
            while (pos < static_cast<int>(bundle.size())) {
                TaskID tId = bundle[pos++];
                auto taskIt = s.tasks.find(tId);
                if (taskIt == s.tasks.end() ||
                    taskIt->second.status != TaskStatus::PENDING) continue;
                const Robot& robot = s.robots.at(rId);
                const TaskInfo& tInfo = sc->getTasks().at(tId);
                Distance dist = sc->distanceBetween(robot.node, tInfo.node);
                BatteryLevel cost = (dist / sc->robots.at(rId).navigationVelocity)
                                  * sc->robots.at(rId).batteryRateWhileNavigating;
                if (cost <= robot.batteryLevel)
                    return Action(Action::Type::EXECUTE_TASK, tId);
            }
        }
        return rolloutAction(rId, s, blockingProb, sc);
    }

    // =========================================================================
    // Física del rollout (idéntica a v2)
    // =========================================================================
    void physicsTask(RobotID rId, TaskID tId, RolloutState& s, EventQueue& q,
                     const std::shared_ptr<const Scenario>& sc) {
        Robot& robot = s.robots.at(rId);
        Task&  task  = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);
        if (task.status != TaskStatus::PENDING ||
            task.assignedWorkers >= tInfo.requiredWorkers) {
            robot.time = s.globalTime + 1.0;
            pushDecision(q, robot.time, rId);
            return;
        }
        NodeID src = robot.node, dst = tInfo.node;
        Time tt = travelTime(rId, src, dst, sc);
        BatteryLevel bat = batteryAfterNav(rId, robot.batteryLevel, src, dst, sc);
        if (bat < 0.0) {
            robot.status = RobotStatus::FAILED;
            robot.batteryLevel = 0.0;
            return;
        }
        Time arrival = s.globalTime + tt;
        robot.batteryLevel = bat;
        robot.node = dst;
        robot.status = RobotStatus::WAITING;
        robot.onTask = tId;
        task.assignedWorkers++;
        task.initTime = std::max({task.initTime, arrival, tInfo.earliestStart});
        robot.time = task.initTime;
        if (task.assignedWorkers == tInfo.requiredWorkers) {
            task.status = TaskStatus::ASSIGNED;
            q.push({task.initTime, REType::TASK_START, NULL_ID, tId, 0, 0});
        }
    }

    void physicsRecharge(RobotID rId, RolloutState& s, EventQueue& q,
                          const std::shared_ptr<const Scenario>& sc) {
        Robot& robot = s.robots.at(rId);
        NodeID src = robot.node;
        StationID sid = sc->getNodes().at(src).nearestStation;
        NodeID dst = sc->getStations().at(sid).node;
        Time tt = travelTime(rId, src, dst, sc);
        BatteryLevel bat = batteryAfterNav(rId, robot.batteryLevel, src, dst, sc);
        if (bat < 0.0) {
            robot.status = RobotStatus::FAILED;
            robot.batteryLevel = 0.0;
            return;
        }
        robot.batteryLevel = sc->robots.at(rId).batteryCapacity;
        robot.node = dst;
        robot.time = s.globalTime + tt;
        robot.status = RobotStatus::AVAILABLE;
        robot.onTask = NULL_ID;
        pushDecision(q, robot.time, rId);
    }

    void physicsFinish(RobotID rId, RolloutState& s,
                        const std::shared_ptr<const Scenario>& sc) {
        Robot& robot = s.robots.at(rId);
        NodeID src = robot.node;
        StationID sid = sc->getNodes().at(src).nearestStation;
        NodeID dst = sc->getStations().at(sid).node;
        BatteryLevel bat = batteryAfterNav(rId, robot.batteryLevel, src, dst, sc);
        if (bat < 0.0) {
            robot.status = RobotStatus::FAILED;
            robot.batteryLevel = 0.0;
            return;
        }
        robot.batteryLevel = bat;
        robot.node = dst;
        robot.status = RobotStatus::FINISHED;
    }

    void applyAction(RobotID rId, Action action, RolloutState& s, EventQueue& q,
                     const std::shared_ptr<const Scenario>& sc) {
        switch (action.getType()) {
            case Action::Type::EXECUTE_TASK: physicsTask(rId, action.getTarget(), s, q, sc); break;
            case Action::Type::RECHARGE:     physicsRecharge(rId, s, q, sc); break;
            case Action::Type::FINISH:       physicsFinish(rId, s, sc); break;
            default: break;
        }
    }

    void processTaskStart(TaskID tId, RolloutState& s, EventQueue& q,
                           const std::shared_ptr<const Scenario>& sc) {
        Task& task = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);
        if (task.status == TaskStatus::COMPLETED || task.status == TaskStatus::FAILED) return;
        task.status = TaskStatus::EXECUTING;
        task.attempts++;
        double p = tInfo.successProb;
        bool success = std::uniform_real_distribution<>(0.0, 1.0)(rng) < p;
        double mu    = success ? tInfo.averageSuccessTime : tInfo.averageFailTime;
        double sigma = success ? tInfo.stdSuccessTime     : tInfo.stdFailTime;
        Time execTime = std::max(0.0, std::normal_distribution<>(mu, sigma)(rng));
        BatteryLevel avgDemand = success ? tInfo.averageSuccessDemand : tInfo.averageFailDemand;
        BatteryRate rate = (mu > 0.0) ? (avgDemand / mu) : 0.0;
        for (auto& [wId, w] : s.robots) {
            if (w.onTask != tId) continue;
            w.status = RobotStatus::EXECUTING;
            if (w.batteryLevel - rate * execTime < 0.0) success = false;
        }
        int payload = success ? 1 : 0;
        q.push({task.initTime + execTime, REType::TASK_END, NULL_ID, tId, payload, 0});
    }

    void processTaskEnd(TaskID tId, bool success, RolloutState& s, EventQueue& q,
                        const std::shared_ptr<const Scenario>& sc) {
        Task& task = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);
        Time execTime = std::max(0.0, s.globalTime - task.initTime);
        BatteryLevel avgDemand = success ? tInfo.averageSuccessDemand : tInfo.averageFailDemand;
        Time avgTime  = success ? tInfo.averageSuccessTime : tInfo.averageFailTime;
        BatteryRate rate = (avgTime > 0.0) ? (avgDemand / avgTime) : 0.0;
        BatteryLevel consumption = rate * execTime;
        for (auto& [wId, w] : s.robots) {
            if (w.onTask != tId) continue;
            w.batteryLevel -= consumption;
            if (w.batteryLevel <= 0.0) {
                w.batteryLevel = 0.0;
                w.status = RobotStatus::FAILED;
            } else {
                w.status = RobotStatus::AVAILABLE;
                w.time   = s.globalTime;
                w.onTask = NULL_ID;
                pushDecision(q, s.globalTime, wId);
            }
        }
        task.finalTime = s.globalTime;
        task.status = success ? TaskStatus::COMPLETED : TaskStatus::FAILED;
    }

    void processTaskExpiration(TaskID tId, RolloutState& s, EventQueue& q) {
        Task& task = s.tasks.at(tId);
        if (task.status == TaskStatus::COMPLETED || task.status == TaskStatus::FAILED ||
            task.status == TaskStatus::ASSIGNED  || task.status == TaskStatus::EXECUTING) return;
        task.status = TaskStatus::FAILED;
        for (auto& [wId, w] : s.robots) {
            if (w.onTask != tId) continue;
            w.status = RobotStatus::AVAILABLE;
            w.onTask = NULL_ID;
            w.time   = s.globalTime;
            pushDecision(q, s.globalTime, wId);
        }
    }

    double computeReward(const RolloutState& s,
                          const std::shared_ptr<const Scenario>& sc) const {
        int nTasks = static_cast<int>(sc->getTasks().size());
        if (nTasks == 0) return 0.0;
        int completed = 0;
        for (const auto& [tId, task] : s.tasks)
            if (task.status == TaskStatus::COMPLETED) completed++;
        return static_cast<double>(completed) / nTasks;
    }

    // =========================================================================
    // Auxiliar: descender a chance node si el outcome ya se conoce
    // =========================================================================
    bool descendToChanceIfKnown(MCTSNode*& currentNode,
                                 std::vector<MCTSNode*>& path,
                                 const RolloutState& s) {
        if (currentNode->nodeType != MCTSNode::NodeType::ACTION) return false;
        if (currentNode->action.getType() != Action::Type::EXECUTE_TASK) return false;
        TaskID tId = currentNode->action.getTarget();
        auto it = s.tasks.find(tId);
        if (it == s.tasks.end()) return false;
        TaskStatus st = it->second.status;
        if (st != TaskStatus::COMPLETED && st != TaskStatus::FAILED) return false;
        int outcome = (st == TaskStatus::COMPLETED) ? OUTCOME_SUCCESS : OUTCOME_FAIL;
        MCTSNode* chance = currentNode->findChanceChild(outcome);
        if (!chance) {
            currentNode->children.push_back(MCTSNode::makeChance(outcome, currentNode));
            chance = currentNode->children.back().get();
        }
        path.push_back(chance);
        currentNode = chance;
        return true;
    }

    // =========================================================================
    // Núcleo de la simulación (con chance nodes y pendingOutcome)
    // =========================================================================
    double simulate(const Observation& obs,
                    const std::shared_ptr<const Scenario>& sc,
                    const std::map<RobotID, std::vector<TaskID>>& sampledBundles,
                    const std::unordered_map<TaskID, double>& blockingProb,
                    SimMode mode,
                    std::vector<MCTSNode*>& path) {

        RolloutState s = makeState(obs);
        EventQueue   q = makeEventQueue(s, sc);

        std::map<RobotID, int> bundlePos;
        MCTSNode* currentNode = (mode == SimMode::ACTIVE_ME_WITH_TREE) ? root.get() : nullptr;
        bool inTree = (mode == SimMode::ACTIVE_ME_WITH_TREE);
        if (inTree) path.push_back(root.get());

        // Estado para gestionar el outcome diferido tras EXECUTE_TASK
        bool      pendingOutcome    = false;
        TaskID    pendingTaskId     = NULL_ID;
        MCTSNode* pendingActionNode = nullptr;

        // Caso inicial: root es ACTION-EXECUTE_TASK (típico tras decideNextAction
        // mientras se ejecuta la tarea elegida). Hay dos posibilidades:
        //   a) la tarea ya tiene outcome final en el observation → descender ya
        //   b) la tarea sigue en curso → marcar pendingOutcome y dejar que el
        //      bucle procese TASK_END/TASK_EXPIRATION
        if (inTree && currentNode->nodeType == MCTSNode::NodeType::ACTION &&
            currentNode->action.getType() == Action::Type::EXECUTE_TASK) {
            if (!descendToChanceIfKnown(currentNode, path, s)) {
                TaskID tId = currentNode->action.getTarget();
                auto it = s.tasks.find(tId);
                if (it != s.tasks.end()) {
                    TaskStatus st = it->second.status;
                    if (st == TaskStatus::ASSIGNED || st == TaskStatus::EXECUTING ||
                        st == TaskStatus::PENDING) {
                        pendingOutcome    = true;
                        pendingTaskId     = tId;
                        pendingActionNode = currentNode;
                    } else {
                        inTree = false;
                    }
                } else {
                    inTree = false;
                }
            }
        }

        int eventCount = 0;
        while (!q.empty() && !s.isFinal() && eventCount < MAX_ROLLOUT_EVENTS) {
            RolloutEvent ev = q.top(); q.pop();
            eventCount++;
            if (ev.time > s.globalTime) s.globalTime = ev.time;

            switch (ev.type) {

                case REType::ROBOT_DECISION: {
                    auto robotIt = s.robots.find(ev.robotID);
                    if (robotIt == s.robots.end()) break;
                    Robot& robot = robotIt->second;
                    if (robot.status != RobotStatus::AVAILABLE) break;

                    Action action(Action::Type::FINISH);

                    if (ev.robotID == myId) {
                        if (mode == SimMode::PASSIVE_ME) {
                            action = Action(Action::Type::FINISH);
                        } else if (inTree) {
                            auto avail = availableActions(s, sc);

                            // Buscar acción factible no expandida (sin hijo ACTION)
                            Action toExpand(Action::Type::FINISH);
                            bool hasUnexplored = false;
                            for (const auto& a : avail) {
                                if (!currentNode->findActionChild(a)) {
                                    toExpand = a;
                                    hasUnexplored = true;
                                    break;
                                }
                            }

                            if (hasUnexplored) {
                                currentNode->children.push_back(
                                    MCTSNode::makeAction(toExpand, currentNode));
                                MCTSNode* newNode = currentNode->children.back().get();
                                path.push_back(newNode);
                                currentNode = newNode;
                                action = toExpand;

                                if (action.getType() == Action::Type::EXECUTE_TASK) {
                                    pendingOutcome    = true;
                                    pendingTaskId     = action.getTarget();
                                    pendingActionNode = newNode;
                                    // permanecemos inTree; al recibir el outcome
                                    // descenderemos al chance node
                                } else if (action.getType() == Action::Type::FINISH) {
                                    inTree = false;
                                }
                                // RECHARGE: permanecemos inTree, sin chance node intermedio
                            } else {
                                MCTSNode* best = selectActionChildDUCT(currentNode, avail);
                                if (best) {
                                    path.push_back(best);
                                    currentNode = best;
                                    action = best->action;

                                    if (action.getType() == Action::Type::EXECUTE_TASK) {
                                        pendingOutcome    = true;
                                        pendingTaskId     = action.getTarget();
                                        pendingActionNode = best;
                                    } else if (action.getType() == Action::Type::FINISH) {
                                        inTree = false;
                                    }
                                } else {
                                    action = rolloutAction(myId, s, blockingProb, sc);
                                    inTree = false;
                                }
                            }
                        } else {
                            action = rolloutAction(myId, s, blockingProb, sc);
                        }
                    } else {
                        action = decideOtherRobot(ev.robotID, s, sampledBundles,
                                                   bundlePos, blockingProb, sc);
                    }

                    applyAction(ev.robotID, action, s, q, sc);
                    break;
                }

                case REType::TASK_START:
                    processTaskStart(ev.taskID, s, q, sc);
                    break;

                case REType::TASK_END: {
                    if (inTree && pendingOutcome && ev.taskID == pendingTaskId) {
                        int outcome = (ev.payload == 1) ? OUTCOME_SUCCESS : OUTCOME_FAIL;
                        MCTSNode* chance = pendingActionNode->findChanceChild(outcome);
                        if (!chance) {
                            pendingActionNode->children.push_back(
                                MCTSNode::makeChance(outcome, pendingActionNode));
                            chance = pendingActionNode->children.back().get();
                        }
                        path.push_back(chance);
                        currentNode       = chance;
                        pendingOutcome    = false;
                        pendingTaskId     = NULL_ID;
                        pendingActionNode = nullptr;
                    }
                    processTaskEnd(ev.taskID, (ev.payload == 1), s, q, sc);
                    break;
                }

                case REType::TASK_EXPIRATION: {
                    if (inTree && pendingOutcome && ev.taskID == pendingTaskId) {
                        MCTSNode* chance = pendingActionNode->findChanceChild(OUTCOME_FAIL);
                        if (!chance) {
                            pendingActionNode->children.push_back(
                                MCTSNode::makeChance(OUTCOME_FAIL, pendingActionNode));
                            chance = pendingActionNode->children.back().get();
                        }
                        path.push_back(chance);
                        currentNode       = chance;
                        pendingOutcome    = false;
                        pendingTaskId     = NULL_ID;
                        pendingActionNode = nullptr;
                    }
                    processTaskExpiration(ev.taskID, s, q);
                    break;
                }
            }
        }

        // Caso anómalo: rollout terminó sin cerrar el outcome (p.ej. myId murió
        // navegando por batería antes de TASK_START). Tratar como FAIL.
        if (inTree && pendingOutcome && pendingActionNode) {
            MCTSNode* chance = pendingActionNode->findChanceChild(OUTCOME_FAIL);
            if (!chance) {
                pendingActionNode->children.push_back(
                    MCTSNode::makeChance(OUTCOME_FAIL, pendingActionNode));
                chance = pendingActionNode->children.back().get();
            }
            path.push_back(chance);
        }

        return computeReward(s, sc);
    }

    // =========================================================================
    // Iteración MCTS (estructura idéntica a v2, ahora con chance nodes en path)
    // =========================================================================
    void runIteration(const Observation& obs, const std::shared_ptr<const Scenario>& sc) {
        if (!root)
            root = MCTSNode::makeAction(Action(Action::Type::START), nullptr);

        auto sampledBundles = sampleBundles(obs.getKnownDistributions());
        auto blockingProb   = computeBlockingProb(myId, sampledBundles, sc);

        std::vector<MCTSNode*> path;
        double reward_with = simulate(obs, sc, sampledBundles, blockingProb,
                                      SimMode::ACTIVE_ME_WITH_TREE, path);

        double backprop_value;
        if (useDiffReward_) {
            std::vector<MCTSNode*> dummyPath;
            double reward_without = simulate(obs, sc, sampledBundles, blockingProb,
                                              SimMode::PASSIVE_ME, dummyPath);
            backprop_value = 0.5 * ((reward_with - reward_without) + 1.0);
        } else {
            backprop_value = reward_with;
        }

        // Backprop: N_disc y Q_disc se incrementan en todos los nodos del path
        // (ACTION y CHANCE por igual). N_avail_disc se incrementa solo en la
        // selección del padre, no aquí.
        for (MCTSNode* node : path) {
            node->decayTo(globalTick, gamma_);
            node->N_disc += 1.0;
            node->Q_disc += backprop_value;
        }
        globalTick++;
    }

    // =========================================================================
    // Extrae la distribución del árbol (bundles planos a través de chance nodes)
    // =========================================================================
    // Sigue el camino más visitado: ACTION-EXECUTE_TASK añade su TaskID al
    // bundle, baja al chance más visitado, baja al siguiente ACTION más visitado.
    // RECHARGE y FINISH terminan la traza del bundle.
    Distribution extractDistribution() {
        Distribution dist;
        if (!root) return dist;

        double rootN = 0.0;
        for (auto& c : root->children) {
            if (c->nodeType != MCTSNode::NodeType::ACTION) continue;
            c->decayTo(globalTick, gamma_);
            rootN += c->N_disc;
        }
        if (rootN < 1e-9) return dist;

        for (auto& child : root->children) {
            if (child->nodeType != MCTSNode::NodeType::ACTION) continue;
            if (child->N_disc < 1e-9) continue;

            Bundle bundle;
            MCTSNode* node = child.get();
            while (node && node->nodeType == MCTSNode::NodeType::ACTION &&
                   node->action.getType() == Action::Type::EXECUTE_TASK) {
                bundle.push_back(node->action.getTarget());
                MCTSNode* chance = node->mostVisitedChanceChild(globalTick, gamma_);
                if (!chance) break;
                node = chance->mostVisitedActionChild(globalTick, gamma_);
            }

            double prob = child->N_disc / rootN;
            if (prob > 0.0) dist[bundle] = prob;
        }
        return dist;
    }

    // =========================================================================
    // Avance bietápico del root tras observar el outcome real
    // =========================================================================
    // Se llama al inicio de decideNextAction. Si la última acción ejecutada fue
    // EXECUTE_TASK, el root actual es ese ACTION node; necesitamos descender al
    // chance node correspondiente al outcome ahora observado.
    void advanceRootForObservedOutcome(const Observation& obs) {
        if (!lastWasExecuteTask) return;
        lastWasExecuteTask = false;
        if (!root) return;
        if (root->nodeType != MCTSNode::NodeType::ACTION) return;
        if (root->action.getType() != Action::Type::EXECUTE_TASK) return;
        if (root->action.getTarget() != lastExecuteTaskId) return;

        const auto& tasks = obs.getKnownTasks();
        auto it = tasks.find(lastExecuteTaskId);
        int outcome;
        if (it == tasks.end()) {
            outcome = OUTCOME_FAIL;
        } else {
            TaskStatus st = it->second.status;
            if      (st == TaskStatus::COMPLETED) outcome = OUTCOME_SUCCESS;
            else if (st == TaskStatus::FAILED)    outcome = OUTCOME_FAIL;
            else                                  outcome = OUTCOME_FAIL; // defensa
        }

        std::unique_ptr<MCTSNode> chanceChild;
        for (auto& c : root->children) {
            if (c->nodeType == MCTSNode::NodeType::CHANCE && c->outcome == outcome) {
                chanceChild = std::move(c);
                break;
            }
        }
        if (chanceChild) {
            chanceChild->parent = nullptr;
            MCTSNode::consolidateTimestamps(chanceChild.get(), globalTick, gamma_);
            root = std::move(chanceChild);
        } else {
            // outcome jamás explorado: árbol vacío para esta rama, partir de cero
            root.reset();
        }
    }

public:
    explicit DecMCTSSolverV3(RobotID id, double gamma = 0.999, bool useDiffReward = false)
        : myId(id), gamma_(gamma), useDiffReward_(useDiffReward),
          rng(std::random_device{}()) {}

    bool requiresContinuousPlanning() const override { return true; }

    std::optional<Distribution> performBackgroundPlanning(
        const Observation& obs,
        const std::shared_ptr<const Scenario>& sc) override {

        for (int i = 0; i < ITERATIONS_PER_CALL; ++i)
            runIteration(obs, sc);

        return extractDistribution();
    }

    Action decideNextAction(
        const Observation& obs,
        const std::shared_ptr<const Scenario>& sc) override {

        // 1) Avance bietápico: descender al chance node del outcome observado
        advanceRootForObservedOutcome(obs);

        // 2) Inicialización defensiva si el árbol está vacío o sin hijos ACTION
        bool needsEmergencyIters = false;
        if (!root) {
            root = MCTSNode::makeAction(Action(Action::Type::START), nullptr);
            needsEmergencyIters = true;
        } else {
            bool hasActionChildren = false;
            for (const auto& c : root->children) {
                if (c->nodeType == MCTSNode::NodeType::ACTION) {
                    hasActionChildren = true;
                    break;
                }
            }
            if (!hasActionChildren) needsEmergencyIters = true;
        }
        if (needsEmergencyIters) {
            for (int i = 0; i < EMERGENCY_ITERS; ++i) runIteration(obs, sc);
        }

        // 3) Elegir el hijo ACTION con más visitas descontadas
        MCTSNode* bestChild = root->mostVisitedActionChild(globalTick, gamma_);

        Action chosen = Action(Action::Type::IDLE);
        if (bestChild) {
            chosen = bestChild->action;
        } else {
            // Fallback: política marginal-aware sin información de vecinos
            auto sampledBundles = sampleBundles(obs.getKnownDistributions());
            auto blockingProb   = computeBlockingProb(myId, sampledBundles, sc);
            chosen = rolloutAction(myId, makeState(obs), blockingProb, sc);
        }

        // 4) Re-enraizar al ACTION elegido y consolidar timestamps
        if (bestChild) {
            std::unique_ptr<MCTSNode> newRoot;
            for (auto& c : root->children) {
                if (c.get() == bestChild) { newRoot = std::move(c); break; }
            }
            if (newRoot) {
                newRoot->parent = nullptr;
                MCTSNode::consolidateTimestamps(newRoot.get(), globalTick, gamma_);
                root = std::move(newRoot);
            } else {
                root.reset();
            }
        } else {
            root.reset();
        }

        // 5) Recordar si la acción fue EXECUTE_TASK para el siguiente avance bietápico
        if (chosen.getType() == Action::Type::EXECUTE_TASK) {
            lastWasExecuteTask = true;
            lastExecuteTaskId  = chosen.getTarget();
        } else {
            lastWasExecuteTask = false;
            lastExecuteTaskId  = NULL_ID;
        }

        return chosen;
    }
};

} // namespace tau