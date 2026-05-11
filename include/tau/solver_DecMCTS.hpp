#pragma once

#include "solver_interface.hpp"
#include "utils/estimation.hpp"
#include <queue>
#include <map>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cassert>

namespace tau {

// STOCHASTIC = true  → V1: resolución estocástica, reward = tareas completadas
// STOCHASTIC = false → V2: resolución determinista,  reward = suma de probabilidades de éxito
template<bool STOCHASTIC>
class DecMCTSSolver : public ISolver {

    // =========================================================================
    // Constantes de configuración
    // =========================================================================
    static constexpr double C_EXPLORE          = 1.414;   // sqrt(2)
    static constexpr int    ITERATIONS_PER_CALL = 20;      // iteraciones por latido de fondo
    static constexpr int    EMERGENCY_ITERS     = 200;     // iteraciones extra si no hay árbol
    static constexpr int    MAX_ROLLOUT_EVENTS  = 200000; // guardia anti-bucle

    // =========================================================================
    // Nodo MCTS
    // =========================================================================
    struct MCTSNode {
        Action   action;
        MCTSNode* parent;
        std::vector<std::unique_ptr<MCTSNode>> children;
        int    N = 0;
        double Q = 0.0;

        MCTSNode(Action a, MCTSNode* p) : action(a), parent(p) {}

        // Devuelve el hijo con esta acción (o nullptr)
        MCTSNode* findChild(Action a) const {
            for (const auto& c : children) {
                if (c->action.getType()   == a.getType() &&
                    c->action.getTarget() == a.getTarget())
                    return c.get();
            }
            return nullptr;
        }

        // Crea o recupera el hijo con esta acción
        MCTSNode* getOrAddChild(Action a) {
            MCTSNode* existing = findChild(a);
            if (existing) return existing;
            children.push_back(std::make_unique<MCTSNode>(a, this));
            return children.back().get();
        }

        // Puntuación UCT del propio nodo (vista desde el padre)
        double uctScore() const {
            if (N == 0) return std::numeric_limits<double>::infinity();
            int pN = parent ? parent->N : N;
            return (Q / N) + C_EXPLORE * std::sqrt(std::log(pN) / N);
        }

        // Hijo con mayor UCT
        MCTSNode* bestUCTChild() const {
            MCTSNode* best = nullptr;
            double bestS = -std::numeric_limits<double>::infinity();
            for (const auto& c : children) {
                double s = c->uctScore();
                if (s > bestS) { bestS = s; best = c.get(); }
            }
            return best;
        }

        // Hijo con más visitas (política de explotación final)
        MCTSNode* mostVisitedChild() const {
            MCTSNode* best = nullptr;
            for (const auto& c : children) {
                if (!best || c->N > best->N) best = c.get();
            }
            return best;
        }
    };

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

    // =========================================================================
    // Miembros del solver
    // =========================================================================
    RobotID myId;
    std::unique_ptr<MCTSNode> root;
    mutable std::mt19937 rng;

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

        // Decisiones para robots DISPONIBLES
        for (const auto& [rId, robot] : s.robots) {
            if (robot.status == RobotStatus::AVAILABLE) {
                Time t = std::max(s.globalTime, robot.time);
                pushDecision(q, t, rId);
            }
            // WAITING/EXECUTING: los libera el evento TASK_END o TASK_EXPIRATION
        }

        // Eventos de tareas
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
                                + (1.0-p) * tInfo.averageFailTime;
                    Time endT = std::max(s.globalTime + 0.001, task.initTime + expDur);
                    int  payload;
                    if constexpr (STOCHASTIC) {
                        payload = (std::uniform_real_distribution<>(0.0,1.0)(rng) < p) ? 1 : 0;
                    } else {
                        payload = 1;
                    }
                    q.push({endT, REType::TASK_END, NULL_ID, tId, payload, 0});
                    break;
                }

                default: break;
            }
        }
        return q;
    }

    // =========================================================================
    // Decisiones: greedy y basada en distribuciones
    // =========================================================================

    Action greedyAction(RobotID rId, const RolloutState& s,
                        const std::shared_ptr<const Scenario>& sc) const {
        const Robot& robot = s.robots.at(rId);
        bool anyPending     = false;
        bool anyUnreachable = false;
        TaskID bestTask     = NULL_ID;
        Distance minDist    = std::numeric_limits<Distance>::infinity();

        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::PENDING) continue;
            anyPending = true;

            const TaskInfo& tInfo = sc->getTasks().at(tId);
            Distance dist = sc->distanceBetween(robot.node, tInfo.node);
            Time tt       = dist / sc->robots.at(rId).navigationVelocity;
            BatteryLevel cost = tt * sc->robots.at(rId).batteryRateWhileNavigating;

            if (cost > robot.batteryLevel) { anyUnreachable = true; continue; }
            if (robot.time + tt > tInfo.latestStart) continue; // miss time window

            if (dist < minDist) { minDist = dist; bestTask = tId; }
        }

        if (!anyPending)           return Action(Action::Type::FINISH);
        if (bestTask == NULL_ID) {
            if (anyUnreachable)    return Action(Action::Type::RECHARGE);
            return                        Action(Action::Type::FINISH);
        }
        return Action(Action::Type::EXECUTE_TASK, bestTask);
    }

    // Acciones disponibles para el robot planificador (para expandir en el árbol)
    std::vector<Action> availableActions(const RolloutState& s,
                                          const std::shared_ptr<const Scenario>& sc) const {
        const Robot& robot = s.robots.at(myId);
        std::vector<Action> actions;
        bool anyPending     = false;
        bool anyUnreachable = false;

        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::PENDING) continue;
            anyPending = true;

            const TaskInfo& tInfo = sc->getTasks().at(tId);
            Distance dist = sc->distanceBetween(robot.node, tInfo.node);
            Time tt       = dist / sc->robots.at(myId).navigationVelocity;
            BatteryLevel cost = tt * sc->robots.at(myId).batteryRateWhileNavigating;

            if (cost > robot.batteryLevel) { anyUnreachable = true; continue; }
            if (robot.time + tt > tInfo.latestStart) { anyUnreachable = true; continue; }

            actions.push_back(Action(Action::Type::EXECUTE_TASK, tId));
        }

        if (!anyPending)    return {Action(Action::Type::FINISH)};
        if (actions.empty()) {
            if (anyUnreachable) return {Action(Action::Type::RECHARGE)};
            return                     {Action(Action::Type::FINISH)};
        }
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

    // Decide la acción de un robot vecino: sigue su bundle muestreado o usa greedy
    Action decideOtherRobot(RobotID rId, const RolloutState& s,
                             const std::map<RobotID, std::vector<TaskID>>& sampledBundles,
                             std::map<RobotID, int>& bundlePos,
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
        return greedyAction(rId, s, sc);
    }

    // =========================================================================
    // Física del rollout (espejo de simulator.cpp sin logs)
    // =========================================================================

    void physicsTask(RobotID rId, TaskID tId, RolloutState& s, EventQueue& q,
                     const std::shared_ptr<const Scenario>& sc) {
        Robot& robot  = s.robots.at(rId);
        Task&  task   = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);

        // Barrera de defensa: tarea no disponible
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
        robot.time = task.initTime; // estimación conservadora de cuándo quedará libre

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

        Time execTime;
        int  success_payload;

        if constexpr (STOCHASTIC) {
            double p = tInfo.successProb;
            bool success = std::uniform_real_distribution<>(0.0,1.0)(rng) < p;
            double mu    = success ? tInfo.averageSuccessTime : tInfo.averageFailTime;
            double sigma = success ? tInfo.stdSuccessTime     : tInfo.stdFailTime;
            execTime = std::max(0.0, std::normal_distribution<>(mu, sigma)(rng));

            BatteryLevel avgDemand = success ? tInfo.averageSuccessDemand : tInfo.averageFailDemand;
            BatteryRate  rate      = (mu > 0.0) ? (avgDemand / mu) : 0.0;

            // Verificar batería y posiblemente forzar fallo
            for (auto& [wId, w] : s.robots) {
                if (w.onTask != tId) continue;
                w.status = RobotStatus::EXECUTING;
                if (w.batteryLevel - rate * execTime < 0.0) {
                    success = false;
                    success_payload = 0;
                }
            }
            success_payload = success ? 1 : 0;
        } else {
            double p    = tInfo.successProb;
            execTime    = p * tInfo.averageSuccessTime + (1.0-p) * tInfo.averageFailTime;
            success_payload = 1;
            for (auto& [wId, w] : s.robots)
                if (w.onTask == tId) w.status = RobotStatus::EXECUTING;
        }

        q.push({task.initTime + execTime, REType::TASK_END, NULL_ID, tId, success_payload, 0});
    }

    // Retorna recompensa incremental para V2 (0 para V1, se calcula al final)
    double processTaskEnd(TaskID tId, bool success, RolloutState& s, EventQueue& q,
                           const std::shared_ptr<const Scenario>& sc) {
        Task& task = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);

        Time execTime  = std::max(0.0, s.globalTime - task.initTime);
        BatteryLevel avgDemand = success ? tInfo.averageSuccessDemand : tInfo.averageFailDemand;
        Time          avgTime  = success ? tInfo.averageSuccessTime   : tInfo.averageFailTime;
        BatteryRate   rate     = (avgTime > 0.0) ? (avgDemand / avgTime) : 0.0;
        BatteryLevel  consumption = rate * execTime;

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

        if constexpr (!STOCHASTIC) {
            return tInfo.successProb; // V2: acumular p para la recompensa final
        }
        return 0.0;
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
    // Cálculo de recompensa al final del rollout
    // =========================================================================
    double computeReward(const RolloutState& s, double accV2,
                          const std::shared_ptr<const Scenario>& sc) const {
        int nTasks = static_cast<int>(sc->getTasks().size());
        if (nTasks == 0) return 0.0;

        if constexpr (STOCHASTIC) {
            int completed = 0;
            for (const auto& [tId, task] : s.tasks)
                if (task.status == TaskStatus::COMPLETED) completed++;
            return static_cast<double>(completed) / nTasks;
        } else {
            return accV2 / nTasks;
        }
    }

    // =========================================================================
    // Iteración MCTS principal
    // =========================================================================
    void runIteration(const Observation& obs, const std::shared_ptr<const Scenario>& sc) {
        if (!root)
            root = std::make_unique<MCTSNode>(Action(Action::Type::START), nullptr);

        RolloutState s = makeState(obs);
        EventQueue   q = makeEventQueue(s, sc);

        auto sampledBundles = sampleBundles(obs.getKnownDistributions());
        std::map<RobotID, int> bundlePos;

        // Contexto de traversal del árbol
        MCTSNode* currentNode = root.get();
        bool inTree = true;
        std::vector<MCTSNode*> path = {root.get()}; // nodos visitados para backprop
        double accV2 = 0.0;
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
                        if (inTree) {
                            auto avail = availableActions(s, sc);

                            // Buscar acción no explorada en el árbol
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
                                // Expansión: nuevo hijo
                                MCTSNode* newNode = currentNode->getOrAddChild(toExpand);
                                path.push_back(newNode);
                                currentNode = newNode;
                                action  = toExpand;
                                inTree  = false; // a partir de aquí: rollout
                            } else if (!currentNode->children.empty()) {
                                // Selección UCT entre hijos ya visitados
                                MCTSNode* best = currentNode->bestUCTChild();
                                if (best) {
                                    path.push_back(best);
                                    currentNode = best;
                                    action = best->action;
                                } else {
                                    action = greedyAction(myId, s, sc);
                                    inTree = false;
                                }
                            } else {
                                // Nodo terminal en el árbol sin hijos ni acciones
                                action = greedyAction(myId, s, sc);
                                inTree = false;
                            }
                        } else {
                            // Fase de rollout: greedy para el robot planificador
                            action = greedyAction(myId, s, sc);
                        }
                    } else {
                        // Robot vecino: bundle muestreado o greedy
                        action = decideOtherRobot(ev.robotID, s, sampledBundles, bundlePos, sc);
                    }

                    applyAction(ev.robotID, action, s, q, sc);
                    break;
                }

                case REType::TASK_START:
                    processTaskStart(ev.taskID, s, q, sc);
                    break;

                case REType::TASK_END:
                    accV2 += processTaskEnd(ev.taskID, (ev.payload == 1), s, q, sc);
                    break;

                case REType::TASK_EXPIRATION:
                    processTaskExpiration(ev.taskID, s, q);
                    break;
            }
        }

        double reward = computeReward(s, accV2, sc);

        // Retropropagación
        for (MCTSNode* node : path) {
            node->N++;
            node->Q += reward;
        }
    }

    // =========================================================================
    // Extrae la distribución del árbol actual (visitas normalizadas)
    // =========================================================================
    Distribution extractDistribution() const {
        Distribution dist;
        if (!root || root->N == 0) return dist;

        for (const auto& child : root->children) {
            if (child->N == 0) continue;

            // Construir el bundle siguiendo el camino más visitado desde este hijo
            Bundle bundle;
            const MCTSNode* node = child.get();
            while (node && node->action.getType() == Action::Type::EXECUTE_TASK) {
                bundle.push_back(node->action.getTarget());
                const MCTSNode* next = node->mostVisitedChild();
                node = next;
            }

            double prob = static_cast<double>(child->N) / root->N;
            if (prob > 0.0) dist[bundle] = prob;
        }
        return dist;
    }

public:
    explicit DecMCTSSolver(RobotID id)
        : myId(id), rng(std::random_device{}()) {}

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

        // Si el árbol está vacío o sin hijos explorados, correr iteraciones de emergencia
        if (!root || root->children.empty() || root->N == 0) {
            if (!root)
                root = std::make_unique<MCTSNode>(Action(Action::Type::START), nullptr);
            for (int i = 0; i < EMERGENCY_ITERS; ++i)
                runIteration(obs, sc);
        }

        // Seleccionar la acción con más visitas (política robusta)
        MCTSNode* bestChild = root->mostVisitedChild();
        Action chosen = bestChild ? bestChild->action
                                   : greedyAction(myId, makeState(obs), sc);

        // Re-enraizar el árbol en el hijo elegido para conservar la exploración futura
        if (bestChild) {
            // Extraer el hijo elegido del root
            std::unique_ptr<MCTSNode> newRoot;
            for (auto& c : root->children) {
                if (c.get() == bestChild) {
                    newRoot = std::move(c);
                    break;
                }
            }
            if (newRoot) {
                newRoot->parent = nullptr;
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

// Alias de conveniencia
using DecMCTSSolverV1 = DecMCTSSolver<true>;   // Estocástico
using DecMCTSSolverV2 = DecMCTSSolver<false>;  // Determinista

} // namespace tau
