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

namespace tau {

class DecMCTSSolverV2 : public ISolver {

    // =========================================================================
    // Constantes de configuración
    // =========================================================================
    // C_EXPLORE (Cp del paper D-UCT): debe ser > 1/sqrt(8) ≈ 0.354.
    //   0.7 ≈ 1/sqrt(2) es buen punto de partida con rewards en [0, 1].
    // Los parámetros gamma y useDiffReward son por instancia (ver constructor).
    static constexpr double C_EXPLORE           = 0.7;
    static constexpr int    ITERATIONS_PER_CALL  = 30;
    static constexpr int    EMERGENCY_ITERS      = 300;
    static constexpr int    MAX_ROLLOUT_EVENTS   = 200000;

    // =========================================================================
    // Nodo MCTS con estadísticas descontadas y descuento perezoso
    // =========================================================================
    struct MCTSNode {
        Action   action;
        MCTSNode* parent;
        std::vector<std::unique_ptr<MCTSNode>> children;

        // Estadísticas descontadas (sustituyen a N, Q originales).
        // N_disc = sum_{u=1..t} gamma^(t-u) * 1{este nodo visitado en u}
        // Q_disc = sum_{u=1..t} gamma^(t-u) * reward_u * 1{este nodo visitado en u}
        double N_disc   = 0.0;
        double Q_disc   = 0.0;
        // Último tick global en el que se aplicó el factor de descuento.
        // Permite descontar perezosamente sin tocar todos los hermanos.
        int    lastTick = 0;

        MCTSNode(Action a, MCTSNode* p) : action(a), parent(p) {}

        MCTSNode* findChild(Action a) const {
            for (const auto& c : children) {
                if (c->action.getType()   == a.getType() &&
                    c->action.getTarget() == a.getTarget())
                    return c.get();
            }
            return nullptr;
        }

        MCTSNode* getOrAddChild(Action a) {
            MCTSNode* existing = findChild(a);
            if (existing) return existing;
            children.push_back(std::make_unique<MCTSNode>(a, this));
            return children.back().get();
        }

        // Aplica gamma^(currentTick - lastTick) perezosamente al nodo.
        // Usa una tabla cacheada por valor de gamma para evitar std::pow (~30 ns/llamada).
        // La tabla se inicializa una vez por valor de gamma distinto encontrado
        // (en simulaciones de agente único hay un único gamma → la rama de reconstrucción
        //  nunca vuelve a ejecutarse tras la primera llamada).
        void decayTo(int currentTick, double gamma) {
            if (currentTick <= lastTick) return;
            int delta = currentTick - lastTick;

            static double   cachedGamma = -1.0;
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
            if (factor < 1e-15) { N_disc = 0.0; Q_disc = 0.0; }
            else                { N_disc *= factor; Q_disc *= factor; }
            lastTick = currentTick;
        }

        // Hijo con mayor visitas descontadas (política de explotación final).
        // Descuenta a todos los hijos al tick actual antes de comparar.
        MCTSNode* mostVisitedChild(int currentTick, double gamma) {
            MCTSNode* best = nullptr;
            double bestN = -1.0;
            for (auto& c : children) {
                c->decayTo(currentTick, gamma);
                if (c->N_disc > bestN) { bestN = c->N_disc; best = c.get(); }
            }
            return best;
        }

        // H3 fix: tras re-enraizar, aplica el descuento pendiente a todo el subárbol
        // y fija lastTick = currentTick. Así la información acumulada se conserva
        // como "fresca" desde el tick de la decisión y D-UCT no la borra de golpe
        // en la primera iteración de la siguiente ronda.
        static void consolidateTimestamps(MCTSNode* node, int currentTick, double gamma) {
            if (!node) return;
            node->decayTo(currentTick, gamma); // aplica gamma^(currentTick-lastTick) y fija lastTick
            for (auto& child : node->children)
                consolidateTimestamps(child.get(), currentTick, gamma);
        }
    };

    // Selecciona el hijo según la fórmula D-UCT.
    // Garantiza expansión de hijos no visitados (N_disc ≈ 0) devolviéndolos primero.
    MCTSNode* selectChildDUCT(MCTSNode* node) {
        // 1) descontar todos los hijos al tick global actual y sumar N_disc del padre
        double parentN = 0.0;
        for (auto& c : node->children) {
            c->decayTo(globalTick, gamma_);
            parentN += c->N_disc;
        }

        // 2) si hay algún hijo "fresco" (N_disc ≈ 0), elegirlo (equivalente a +inf en UCT)
        for (auto& c : node->children) {
            if (c->N_disc < 1e-9) return c.get();
        }

        // 3) en otro caso, fórmula D-UCT estándar:
        //    U_j = Q_disc_j / N_disc_j  +  2 * Cp * sqrt(log(parentN) / N_disc_j)
        MCTSNode* best = nullptr;
        double bestU = -std::numeric_limits<double>::infinity();
        const double logParent = std::log(std::max(parentN, 1.0));
        for (auto& c : node->children) {
            double Fbar  = c->Q_disc / c->N_disc;
            double bonus = 2.0 * C_EXPLORE * std::sqrt(logParent / c->N_disc);
            double U     = Fbar + bonus;
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

    // =========================================================================
    // Eventos del rollout (espejo de EventType en simulator.hpp)
    // =========================================================================
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

    // Modo de simulación para el doble rollout (utilidad local f^r):
    //   ACTIVE_ME_WITH_TREE: el robot myId actúa, se selecciona/expande el árbol.
    //                        Devuelve el path para backprop.
    //   PASSIVE_ME:          el robot myId hace FINISH inmediato. No se toca el árbol.
    //                        Sirve para calcular el baseline g(x^r_∅ ∪ x^(r)).
    enum class SimMode {
        ACTIVE_ME_WITH_TREE,
        PASSIVE_ME
    };

    // =========================================================================
    // Miembros del solver
    // =========================================================================
    RobotID myId;
    // Parámetros configurables por instancia (para ablaciones en tiempo de ejecución)
    double gamma_;          // factor de descuento D-UCT (típico: [0.95, 0.999])
    bool   useDiffReward_;  // true → f^r = reward_with − reward_without; false → reward_with directo
    std::unique_ptr<MCTSNode> root;
    mutable std::mt19937 rng;
    int globalTick = 0;   // se incrementa una vez por iteración completa de MCTS

    // =========================================================================
    // Utilidades de física (espejo de simulator.cpp sin logs)
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

    // =========================================================================
    // Construcción del estado y la cola inicial
    // =========================================================================

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
    // Pre-cómputo de probabilidad de bloqueo por tarea
    // =========================================================================
    // Para cada tarea, estimación de la probabilidad de que sea completada
    // POR LOS OTROS robots (excluyendo a excludeId) según los bundles muestreados.
    // Versión bundle-consistente: usa las muestras concretas del rollout, no las
    // distribuciones marginales. Coste O(|T| * R * K) una sola vez por rollout.
    //
    // Tratamiento de requiredWorkers:
    //   - Si #{vecinos con la tarea en su bundle} >= requiredWorkers, blockingProb = successProb.
    //   - En otro caso, blockingProb = 0 (no podrán completarla sin mí, así que vale la pena
    //     contemplarla en mi plan).
    std::unordered_map<TaskID, double> computeBlockingProb(
        RobotID excludeId,
        const std::map<RobotID, std::vector<TaskID>>& sampledBundles,
        const std::shared_ptr<const Scenario>& sc) const {

        // 1) contar, para cada tarea, cuántos OTROS robots la tienen en su bundle
        std::unordered_map<TaskID, int> countOthers;
        for (const auto& [rId, bundle] : sampledBundles) {
            if (rId == excludeId) continue;
            // Para evitar contar dos veces si una misma tarea aparece duplicada en un bundle
            std::set<TaskID> seen;
            for (TaskID tId : bundle) {
                if (seen.insert(tId).second) countOthers[tId]++;
            }
        }

        // 2) traducir a blocking probability
        std::unordered_map<TaskID, double> result;
        result.reserve(sc->getTasks().size());
        for (const auto& [tId, tInfo] : sc->getTasks()) {
            auto it = countOthers.find(tId);
            int n = (it == countOthers.end()) ? 0 : it->second;
            if (n >= tInfo.requiredWorkers) {
                result[tId] = tInfo.successProb;
            } else {
                result[tId] = 0.0;
            }
        }
        return result;
    }

    // =========================================================================
    // Política de rollout marginal-aware
    // =========================================================================
    // Sustituye al antiguo greedyAction como política por defecto en los rollouts.
    // Para cada tarea factible, calcula:
    //   valor_marginal(t) = p_succ(t) * (1 - blockingProb(t))     [con w_k = 1]
    //   coste(t)          = tiempo_viaje(robot -> t) + tiempo_medio_ejecucion(t)
    //   score(t)          = valor_marginal(t) / max(coste, eps)
    // Selecciona la tarea con mayor score, RECHARGE si hay tareas pero ninguna alcanzable
    // por batería, o FINISH si no quedan tareas pendientes.
    Action rolloutAction(RobotID rId, const RolloutState& s,
                          const std::unordered_map<TaskID, double>& blockingProb,
                          const std::shared_ptr<const Scenario>& sc) const {
        const Robot& robot   = s.robots.at(rId);
        const double battCap = sc->robots.at(rId).batteryCapacity;
        bool anyPending            = false;
        bool anyBatteryUnreachable = false;

        TaskID bestTask  = NULL_ID;
        double bestScore = -std::numeric_limits<double>::infinity();

        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::PENDING) continue;
            anyPending = true;

            const TaskInfo& tInfo = sc->getTasks().at(tId);
            Distance dist  = sc->distanceBetween(robot.node, tInfo.node);
            Time tt        = dist / sc->robots.at(rId).navigationVelocity;
            BatteryLevel cBat = tt * sc->robots.at(rId).batteryRateWhileNavigating;

            // La ventana temporal ha expirado: recargar no ayuda (avanza el tiempo).
            if (robot.time + tt > tInfo.latestStart) continue;
            if (cBat > robot.batteryLevel) { anyBatteryUnreachable = true; continue; }

            // valor marginal según el paper (con w_k = 1 en tu MRTAU)
            double pSelf   = tInfo.successProb;
            auto itBlock   = blockingProb.find(tId);
            double pBlock  = (itBlock == blockingProb.end()) ? 0.0 : itBlock->second;
            double value   = pSelf * (1.0 - pBlock);

            // coste efectivo: viaje + ejecución esperada
            double execExp = pSelf * tInfo.averageSuccessTime
                           + (1.0 - pSelf) * tInfo.averageFailTime;
            double cost    = std::max(0.1, tt + execExp);
            double score   = value / cost;

            if (score > bestScore) { bestScore = score; bestTask = tId; }
        }

        if (!anyPending)   return Action(Action::Type::FINISH);
        if (bestTask == NULL_ID) {
            // Solo recargar si la batería no está ya llena; evita bucle infinito
            // cuando la distancia a todas las tareas supera batteryCapacity.
            if (anyBatteryUnreachable && robot.batteryLevel < battCap * 0.99)
                return Action(Action::Type::RECHARGE);
            return Action(Action::Type::FINISH);
        }
        return Action(Action::Type::EXECUTE_TASK, bestTask);
    }

    // Acciones disponibles para el robot planificador (para expandir en el árbol)
    std::vector<Action> availableActions(const RolloutState& s,
                                          const std::shared_ptr<const Scenario>& sc) const {
        const Robot& robot   = s.robots.at(myId);
        const double battCap = sc->robots.at(myId).batteryCapacity;
        std::vector<Action> actions;
        bool anyPending            = false;
        bool anyBatteryUnreachable = false;

        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::PENDING) continue;
            anyPending = true;

            const TaskInfo& tInfo = sc->getTasks().at(tId);
            Distance dist = sc->distanceBetween(robot.node, tInfo.node);
            Time tt       = dist / sc->robots.at(myId).navigationVelocity;
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
        // Solo ofrecer RECHARGE si la batería no está llena: si ya está al máximo,
        // RECHARGE en la estación es un no-op (travelTime=0, robot.time no avanza)
        // y el árbol puede seleccionarlo indefinidamente causando un bucle infinito
        // en el simulador real.
        if (robot.batteryLevel < battCap * 0.99)
            actions.push_back(Action(Action::Type::RECHARGE));
        return actions;
    }

    // Muestrea bundles de la distribución de cada robot vecino
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

    // Decide la acción de un robot vecino: sigue su bundle muestreado; si se agota,
    // usa la política de rollout marginal-aware (en vez del greedy original).
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
        // Fallback: política marginal-aware en lugar de greedy.
        // Nota: blockingProb está calculada excluyendo a myId, no a rId. Es una
        // aproximación: el sesgo es pequeño porque los vecinos en su fallback no
        // están "razonando estratégicamente" sino completando su plan residual.
        return rolloutAction(rId, s, blockingProb, sc);
    }

    // =========================================================================
    // Física del rollout (sin cambios respecto a la versión original)
    // =========================================================================

    void physicsTask(RobotID rId, TaskID tId, RolloutState& s, EventQueue& q,
                     const std::shared_ptr<const Scenario>& sc) {
        Robot& robot  = s.robots.at(rId);
        Task&  task   = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);

        if (task.status != TaskStatus::PENDING ||
            task.assignedWorkers >= tInfo.requiredWorkers) {
            robot.time = s.globalTime + 1.0;
            pushDecision(q, robot.time, rId);
            return;
        }

        NodeID src = robot.node, dst = tInfo.node;
        Time tt      = travelTime(rId, src, dst, sc);
        BatteryLevel bat = batteryAfterNav(rId, robot.batteryLevel, src, dst, sc);

        if (bat < 0.0) {
            robot.status      = RobotStatus::FAILED;
            robot.batteryLevel = 0.0;
            return;
        }

        Time arrival = s.globalTime + tt;
        robot.batteryLevel = bat;
        robot.node         = dst;
        robot.status       = RobotStatus::WAITING;
        robot.onTask       = tId;

        task.assignedWorkers++;
        task.initTime = std::max({task.initTime, arrival, tInfo.earliestStart});
        // Fidelidad con simulator.cpp::simulateTask: el robot se da por ocupado
        // hasta el cierre de la ventana mientras espera a su coalición.
        robot.time = tInfo.latestStart;

        if (task.assignedWorkers == tInfo.requiredWorkers) {
            task.status = TaskStatus::ASSIGNED;
            q.push({task.initTime, REType::TASK_START, NULL_ID, tId, 0, 0});
        }
    }

    void physicsRecharge(RobotID rId, RolloutState& s, EventQueue& q,
                          const std::shared_ptr<const Scenario>& sc) {
        Robot& robot = s.robots.at(rId);
        NodeID src   = robot.node;
        StationID sid = sc->getNodes().at(src).nearestStation;
        NodeID dst   = sc->getStations().at(sid).node;

        Time tt      = travelTime(rId, src, dst, sc);
        BatteryLevel bat = batteryAfterNav(rId, robot.batteryLevel, src, dst, sc);

        if (bat < 0.0) {
            robot.status      = RobotStatus::FAILED;
            robot.batteryLevel = 0.0;
            return;
        }

        robot.batteryLevel = sc->robots.at(rId).batteryCapacity;
        robot.node         = dst;
        robot.time         = s.globalTime + tt;
        robot.status       = RobotStatus::AVAILABLE;
        robot.onTask       = NULL_ID;
        pushDecision(q, robot.time, rId);
    }

    void physicsFinish(RobotID rId, RolloutState& s,
                        const std::shared_ptr<const Scenario>& sc) {
        Robot& robot = s.robots.at(rId);
        NodeID src   = robot.node;
        StationID sid = sc->getNodes().at(src).nearestStation;
        NodeID dst   = sc->getStations().at(sid).node;

        BatteryLevel bat = batteryAfterNav(rId, robot.batteryLevel, src, dst, sc);

        if (bat < 0.0) {
            robot.status      = RobotStatus::FAILED;
            robot.batteryLevel = 0.0;
            return;
        }
        robot.batteryLevel = bat;
        robot.node         = dst;
        robot.status       = RobotStatus::FINISHED;
    }

    void applyAction(RobotID rId, Action action, RolloutState& s, EventQueue& q,
                     const std::shared_ptr<const Scenario>& sc) {
        switch (action.getType()) {
            case Action::Type::EXECUTE_TASK:
                physicsTask(rId, action.getTarget(), s, q, sc);
                break;
            case Action::Type::RECHARGE:
                physicsRecharge(rId, s, q, sc);
                break;
            case Action::Type::FINISH:
                physicsFinish(rId, s, sc);
                break;
            default: break;
        }
    }

    void processTaskStart(TaskID tId, RolloutState& s, EventQueue& q,
                           const std::shared_ptr<const Scenario>& sc) {
        Task& task = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);

        if (task.status == TaskStatus::COMPLETED ||
            task.status == TaskStatus::FAILED) return;

        task.status = TaskStatus::EXECUTING;
        task.attempts++;

        double p = tInfo.successProb;
        bool success = std::uniform_real_distribution<>(0.0, 1.0)(rng) < p;
        double mu    = success ? tInfo.averageSuccessTime : tInfo.averageFailTime;
        double sigma = success ? tInfo.stdSuccessTime     : tInfo.stdFailTime;
        Time execTime = std::max(0.0, std::normal_distribution<>(mu, sigma)(rng));

        BatteryLevel avgDemand = success ? tInfo.averageSuccessDemand : tInfo.averageFailDemand;
        BatteryRate  rate      = (mu > 0.0) ? (avgDemand / mu) : 0.0;

        for (auto& [wId, w] : s.robots) {
            if (w.onTask != tId) continue;
            w.status = RobotStatus::EXECUTING;
            if (w.batteryLevel - rate * execTime < 0.0) {
                success = false;
            }
        }
        int success_payload = success ? 1 : 0;

        q.push({task.initTime + execTime, REType::TASK_END, NULL_ID, tId, success_payload, 0});
    }

    // Limpieza: ya no devuelve double (accV2 era código muerto).
    void processTaskEnd(TaskID tId, bool success, RolloutState& s, EventQueue& q,
                        const std::shared_ptr<const Scenario>& sc) {
        Task& task = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);

        Time execTime  = std::max(0.0, s.globalTime - task.initTime);
        BatteryLevel avgDemand = success ? tInfo.averageSuccessDemand : tInfo.averageFailDemand;
        Time          avgTime  = success ? tInfo.averageSuccessTime   : tInfo.averageFailTime;
        // Fidelidad con simulator.cpp::endTask: si la duración media de este
        // desenlace es 0 (fail_time=[0,0]), el consumo es la demanda completa.
        BatteryLevel  consumption = (avgTime > 0.0) ? (avgDemand / avgTime) * execTime
                                                    : avgDemand;

        for (auto& [wId, w] : s.robots) {
            if (w.onTask != tId) continue;
            w.batteryLevel -= consumption;
            if (w.batteryLevel <= 0.0) {
                w.batteryLevel = 0.0;
                w.status       = RobotStatus::FAILED;
            } else {
                w.status  = RobotStatus::AVAILABLE;
                w.time    = s.globalTime;
                w.onTask  = NULL_ID;
                pushDecision(q, s.globalTime, wId);
            }
        }

        task.finalTime = s.globalTime;
        task.status    = success ? TaskStatus::COMPLETED : TaskStatus::FAILED;
    }

    void processTaskExpiration(TaskID tId, RolloutState& s, EventQueue& q) {
        Task& task = s.tasks.at(tId);
        if (task.status == TaskStatus::COMPLETED  ||
            task.status == TaskStatus::FAILED      ||
            task.status == TaskStatus::ASSIGNED    ||
            task.status == TaskStatus::EXECUTING) return;

        task.status = TaskStatus::FAILED;
        for (auto& [wId, w] : s.robots) {
            if (w.onTask != tId) continue;
            w.status = RobotStatus::AVAILABLE;
            w.onTask = NULL_ID;
            w.time   = s.globalTime;
            pushDecision(q, s.globalTime, wId);
        }
    }

    // =========================================================================
    // Recompensa al final del rollout
    // =========================================================================
    // En MRTAU: g = fracción de tareas completadas con éxito.
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
    // Simulación completa parametrizada por modo (núcleo del doble rollout)
    // =========================================================================
    // En modo ACTIVE_ME_WITH_TREE: el robot myId hace selección/expansión/rollout
    //   y se rellena `path` con los nodos visitados (para backprop).
    // En modo PASSIVE_ME: el robot myId hace FINISH inmediato en cada decisión.
    //   `path` queda vacío.
    //
    // Devuelve la recompensa global g al final del rollout.
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
                            // Baseline: el robot "no participa" desde el inicio.
                            action = Action(Action::Type::FINISH);
                        } else if (inTree) {
                            auto avail = availableActions(s, sc);

                            Action toExpand(Action::Type::FINISH);
                            bool   hasUnexplored = false;
                            for (const auto& a : avail) {
                                if (!currentNode->findChild(a)) {
                                    toExpand = a;
                                    hasUnexplored = true;
                                    break;
                                }
                            }

                            if (hasUnexplored) {
                                MCTSNode* newNode = currentNode->getOrAddChild(toExpand);
                                path.push_back(newNode);
                                currentNode = newNode;
                                action  = toExpand;
                                inTree  = false; // a partir de aquí, rollout marginal-aware
                            } else if (!currentNode->children.empty()) {
                                MCTSNode* best = selectChildDUCT(currentNode);
                                if (best) {
                                    path.push_back(best);
                                    currentNode = best;
                                    action = best->action;
                                } else {
                                    action = rolloutAction(myId, s, blockingProb, sc);
                                    inTree = false;
                                }
                            } else {
                                action = rolloutAction(myId, s, blockingProb, sc);
                                inTree = false;
                            }
                        } else {
                            // Fase de rollout para myId: política marginal-aware.
                            action = rolloutAction(myId, s, blockingProb, sc);
                        }
                    } else {
                        // Robot vecino: sigue su bundle muestreado o fallback marginal-aware.
                        action = decideOtherRobot(ev.robotID, s, sampledBundles,
                                                   bundlePos, blockingProb, sc);
                    }

                    applyAction(ev.robotID, action, s, q, sc);
                    break;
                }

                case REType::TASK_START:
                    processTaskStart(ev.taskID, s, q, sc);
                    break;

                case REType::TASK_END:
                    processTaskEnd(ev.taskID, (ev.payload == 1), s, q, sc);
                    break;

                case REType::TASK_EXPIRATION:
                    processTaskExpiration(ev.taskID, s, q);
                    break;
            }
        }

        return computeReward(s, sc);
    }

    // =========================================================================
    // Iteración MCTS
    // =========================================================================
    // Si useDiffReward_=true:  doble rollout → f^r = reward_with − reward_without.
    // Si useDiffReward_=false: rollout único  → backprop reward_with directo.
    // El experimento de ablación (H1, n=85×4 configs) confirmó que
    // useDiffReward_=false es ligeramente mejor (Δ≈+0.012) en escenarios
    // con required_workers=1. El baseline PASSIVE_ME comprime f_r→0 al reducir
    // la competencia entre robots, perdiendo discriminación entre acciones.
    void runIteration(const Observation& obs, const std::shared_ptr<const Scenario>& sc) {
        if (!root)
            root = std::make_unique<MCTSNode>(Action(Action::Type::START), nullptr);

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

        // Retropropagación con descuento perezoso
        for (MCTSNode* node : path) {
            node->decayTo(globalTick, gamma_);
            node->N_disc += 1.0;
            node->Q_disc += backprop_value;
        }
        globalTick++;
    }

    // =========================================================================
    // Extrae la distribución del árbol actual (visitas descontadas normalizadas)
    // =========================================================================
    Distribution extractDistribution() {
        Distribution dist;
        if (!root) return dist;

        // Descontar al tick actual para que las visitas reflejen la información reciente
        double rootN = 0.0;
        for (auto& c : root->children) {
            c->decayTo(globalTick, gamma_);
            rootN += c->N_disc;
        }
        if (rootN < 1e-9) return dist;

        for (auto& child : root->children) {
            if (child->N_disc < 1e-9) continue;

            // Construir el bundle siguiendo el camino más visitado (descontado)
            Bundle bundle;
            MCTSNode* node = child.get();
            while (node && node->action.getType() == Action::Type::EXECUTE_TASK) {
                bundle.push_back(node->action.getTarget());
                MCTSNode* next = node->mostVisitedChild(globalTick, gamma_);
                node = next;
            }

            double prob = child->N_disc / rootN;
            if (prob > 0.0) dist[bundle] = prob;
        }
        return dist;
    }

    // =========================================================================
    // Política de explotación final, filtrada por factibilidad
    // =========================================================================
    // Las visitas descontadas NO son comparables entre hermanos cuando el
    // conjunto de acciones disponibles varía de un rollout a otro: una acción
    // que solo aparece cuando es la única posible (típicamente FINISH) acumula
    // todas las visitas de esos rollouts, mientras las demás se reparten las
    // restantes. Sin filtrar, el robot podía retirarse quedando tareas
    // alcanzables. El filtro es NO destructivo: no borra subárboles.
    MCTSNode* mostVisitedFeasibleChild(const Observation& obs,
                                        const std::shared_ptr<const Scenario>& sc) {
        if (!root || root->children.empty()) return nullptr;
        auto avail = availableActions(makeState(obs), sc);

        MCTSNode* best = nullptr;
        double bestN = -1.0;
        for (auto& c : root->children) {
            bool feasible = std::any_of(avail.begin(), avail.end(), [&](const Action& a) {
                return a.getType()   == c->action.getType() &&
                       a.getTarget() == c->action.getTarget();
            });
            if (!feasible) continue;
            c->decayTo(globalTick, gamma_);
            if (c->N_disc > bestN) { bestN = c->N_disc; best = c.get(); }
        }
        return best;
    }

public:
    // gamma: factor D-UCT (0.95–0.999). useDiffReward: activa el doble rollout f^r.
    // Ablación H1 confirmó que useDiffReward=false es ligeramente mejor en escenarios
    // con required_workers=1; H4 confirmó que gamma=0.999 es muy superior a 0.99.
    explicit DecMCTSSolverV2(RobotID id, double gamma = 0.999, bool useDiffReward = false)
        : myId(id), gamma_(gamma), useDiffReward_(useDiffReward), rng(std::random_device{}()) {}

    // =========================================================================
    // Interfaz ISolver
    // =========================================================================

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

        if (!root || root->children.empty() ||
            (root && root->N_disc < 1e-9 && root->children.empty())) {
            if (!root)
                root = std::make_unique<MCTSNode>(Action(Action::Type::START), nullptr);
            for (int i = 0; i < EMERGENCY_ITERS; ++i)
                runIteration(obs, sc);
        }

        // Política robusta: hijo con más visitas descontadas entre las aplicables
        MCTSNode* bestChild = mostVisitedFeasibleChild(obs, sc);

        Action chosen = Action(Action::Type::IDLE); // Pongo IDLE como valor por defecto, aunque no debería usarse.
        if (bestChild) {
            chosen = bestChild->action;
        } else {
            // Fallback: política de rollout marginal-aware sin información de vecinos
            auto sampledBundles = sampleBundles(obs.getKnownDistributions());
            auto blockingProb   = computeBlockingProb(myId, sampledBundles, sc);
            chosen = rolloutAction(myId, makeState(obs), blockingProb, sc);
        }

        // Re-enraizamiento: conserva el subárbol del hijo elegido.
        // Después de mover, consolida timestamps (H3 fix): aplica el descuento
        // pendiente a todo el subárbol y fija lastTick = globalTick, para que
        // la información acumulada no se borre de golpe en la primera iteración
        // de la siguiente ronda de planificación.
        if (bestChild) {
            std::unique_ptr<MCTSNode> newRoot;
            for (auto& c : root->children) {
                if (c.get() == bestChild) {
                    newRoot = std::move(c);
                    break;
                }
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

        return chosen;
    }
};

} // namespace tau