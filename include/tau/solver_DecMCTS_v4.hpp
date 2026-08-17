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

// =============================================================================
// DecMCTSSolverV4
// =============================================================================
// Evolución de V2 (la mejor versión hasta ahora). Mantiene su núcleo:
//   - D-UCT con descuento perezoso (gamma configurable, por defecto 0.999)
//   - doble rollout opcional para f^r (useDiffReward, por defecto false)
//   - muestreo de bundles vecinos por iteración + blockingProb
//   - consolidateTimestamps tras re-enraizar (H3 fix)
//
// Cambios respecto a V2 (motivados por el análisis de errores y por la
// literatura de TOPTW / Dec-MCTS):
//
// 1) POLÍTICA DE ROLLOUT CONSCIENTE DE VENTANAS TEMPORALES.
//    El score pasa de valor/(viaje+ejecución) a:
//       score = valor / (viaje + ESPERA + ejecución) * (1 + W_u * tau/(tau+slack))
//    donde espera = max(0, earliestStart - llegada), slack = latestStart - llegada
//    y tau = viaje + ejecución (escala propia de la tarea, adimensionaliza el
//    slack). Las tareas cuya ventana se cierra pronto se priorizan (estilo
//    heurísticas de inserción para orienteering con ventanas temporales).
//
// 2) WIDENING PROGRESIVO + EXPANSIÓN ORDENADA POR HEURÍSTICA.
//    V2 expandía el primer hijo no explorado en orden de TaskID y exigía
//    expandir todos los hijos antes de discriminar. Con 12-24 acciones y 30
//    iteraciones por latido el árbol quedaba plano y sesgado. Ahora el número
//    de hijos se limita a 1 + PW_C * N_disc^PW_ALPHA y el siguiente hijo a
//    expandir es el de mayor score heurístico (progressive bias).
//
// 3) SELECCIÓN D-UCT FILTRADA POR FACTIBILIDAD.
//    V2 seleccionaba entre todos los hijos aunque su tarea ya estuviera cogida
//    o caducada en el estado del rollout (la física lo "defendía" con +1s y
//    contaminaba las estadísticas). Ahora la selección solo considera hijos
//    cuya acción está disponible en el estado actual (la parte buena de V3,
//    sin chance nodes ni doble estadística).
//
// 4) PODA DE HIJOS OBSOLETOS DEL ROOT.
//    Al inicio de cada performBackgroundPlanning/decideNextAction se eliminan
//    los hijos del root cuya acción ya es imposible según la Observation
//    (tarea no PENDING, ventana definitivamente inalcanzable, RECHARGE con
//    batería llena). Evita que mostVisitedChild elija acciones muertas y que
//    el robot real pague la penalización defensiva de 1s.
//
// 5) FACTIBILIDAD DE BATERÍA COMPLETA.
//    Para comprometerse con una tarea se exige batería para navegar Y para la
//    demanda esperada de ejecución. En V2 un robot podía llegar con batería
//    insuficiente: la tarea fallaba y el robot moría (catástrofe doble que el
//    árbol solo aprendía vía rollouts).
//
// 6) TIE-BREAK DE EARLINESS EN LA RECOMPENSA.
//    reward = completadas/n + (W_e/n) * earliness_media, con W_e < 1 el bono
//    está estrictamente dominado por completar una tarea más, pero rompe las
//    mesetas (muchos planes completan el mismo número) prefiriendo planes que
//    arrancan las tareas antes dentro de su ventana (libera capacidad).
//
// 7) Corrección menor: extractDistribution acumula (+=) cuando varios hijos
//    mapean al mismo bundle (RECHARGE y FINISH -> bundle vacío).
// =============================================================================

class DecMCTSSolverV4 : public ISolver {

    // =========================================================================
    // Constantes de configuración
    // =========================================================================
    // C_EXPLORE por defecto (configurable por instancia vía constructor)
    static constexpr double DEFAULT_C_EXPLORE    = 0.7;
    static constexpr int    ITERATIONS_PER_CALL  = 30;
    static constexpr int    EMERGENCY_ITERS      = 300;
    static constexpr int    MAX_ROLLOUT_EVENTS   = 200000;

    // Widening progresivo: máx hijos = 1 + PW_C * N_disc^PW_ALPHA
    static constexpr double PW_C     = 1.5;
    static constexpr double PW_ALPHA = 0.6;

    // Peso del factor de urgencia en la política de rollout (cambio 1)
    static constexpr double URGENCY_WEIGHT = 1.0;
    // Peso del bono de earliness en la recompensa (cambio 6). Debe ser < 1
    // para que completar una tarea adicional siempre domine al bono.
    static constexpr double EARLINESS_WEIGHT = 0.5;
    // Score heurístico asignado a RECHARGE: positivo pero mínimo, de forma que
    // el widening lo expanda el último salvo que no haya tareas factibles.
    static constexpr double RECHARGE_SCORE = 1e-6;

    // =========================================================================
    // Nodo MCTS con estadísticas descontadas y descuento perezoso (como V2)
    // =========================================================================
    struct MCTSNode {
        Action   action;
        MCTSNode* parent;
        std::vector<std::unique_ptr<MCTSNode>> children;

        double N_disc   = 0.0;
        double Q_disc   = 0.0;
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

        // Aplica gamma^(currentTick - lastTick) perezosamente (tabla cacheada).
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

        MCTSNode* mostVisitedChild(int currentTick, double gamma) {
            MCTSNode* best = nullptr;
            double bestN = -1.0;
            for (auto& c : children) {
                c->decayTo(currentTick, gamma);
                if (c->N_disc > bestN) { bestN = c->N_disc; best = c.get(); }
            }
            return best;
        }

        static void consolidateTimestamps(MCTSNode* node, int currentTick, double gamma) {
            if (!node) return;
            node->decayTo(currentTick, gamma);
            for (auto& child : node->children)
                consolidateTimestamps(child.get(), currentTick, gamma);
        }
    };

    // Acción candidata con su puntuación heurística (para ordenar expansiones)
    struct ScoredAction {
        Action action;
        double score;
    };

    // =========================================================================
    // Estado del simulador de rollout (idéntico a V2)
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
    double gamma_;
    bool   useDiffReward_;
    double cExplore_;
    int    iterationsPerCall_;   // iteraciones MCTS por latido de planificación
    int    emergencyIters_;      // iteraciones de emergencia si el árbol está vacío
    // Ablación C1: si es false, el solver IGNORA las distribuciones comunicadas por
    // los vecinos. En los rollouts, los demás robots dejan de seguir su bundle
    // muestreado y pasan a regirse por la política de rollout, y blockingProb queda
    // a 0 para todas las tareas (sin de-conflicción). Todo lo demás (árbol, D-UCT,
    // física, publicación de la propia distribución) es idéntico. Sirve para medir
    // cuánto aporta realmente el canal de comunicación de Dec-MCTS.
    bool   useComm_;
    std::unique_ptr<MCTSNode> root;
    mutable std::mt19937 rng;
    int globalTick = 0;

    // =========================================================================
    // Utilidades de física (espejo de simulator.cpp sin logs, como V2)
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
    // Construcción del estado y la cola inicial (idéntico a V2)
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
                    // Espejo del simulador corregido: una tarea ASSIGNED cuyo inicio
                    // (initTime) queda más allá de latestStart caducará en latestStart.
                    if (task.initTime > tInfo.latestStart &&
                        tInfo.latestStart > s.globalTime) {
                        q.push({tInfo.latestStart, REType::TASK_EXPIRATION,
                                NULL_ID, tId, 0, 0});
                    }
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
    // Pre-cómputo de blockingProb (idéntico a V2)
    // =========================================================================
    std::unordered_map<TaskID, double> computeBlockingProb(
        RobotID excludeId,
        const std::map<RobotID, std::vector<TaskID>>& sampledBundles,
        const std::shared_ptr<const Scenario>& sc) const {

        std::unordered_map<TaskID, int> countOthers;
        for (const auto& [rId, bundle] : sampledBundles) {
            if (rId == excludeId) continue;
            std::set<TaskID> seen;
            for (TaskID tId : bundle) {
                if (seen.insert(tId).second) countOthers[tId]++;
            }
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

    // =========================================================================
    // Núcleo heurístico V4 (cambios 1 y 5)
    // =========================================================================
    // Devuelve las tareas factibles para rId con su score heurístico:
    //   llegada  = max(robot.time, globalTime) + viaje
    //   factible si llegada <= latestStart y batería >= navegación + demanda esperada
    //   espera   = max(0, earliestStart - llegada)
    //   slack    = latestStart - llegada
    //   valor    = p_succ * (1 - blockingProb) + epsilon * p_succ   (epsilon
    //              mantiene un orden razonable entre tareas "bloqueadas")
    //   coste    = viaje + espera + ejecución esperada
    //   urgencia = 1 + W_u * tau / (tau + slack),  tau = viaje + ejecución
    //   score    = (valor / coste) * urgencia
    std::vector<ScoredAction> scoreFeasibleTasks(
        RobotID rId, const RolloutState& s,
        const std::unordered_map<TaskID, double>& blockingProb,
        const std::shared_ptr<const Scenario>& sc,
        bool& anyPending, bool& anyBatteryBlocked) const {

        const Robot& robot     = s.robots.at(rId);
        const RobotInfo& rInfo = sc->robots.at(rId);
        anyPending        = false;
        anyBatteryBlocked = false;

        std::vector<ScoredAction> out;
        Time now = std::max(robot.time, s.globalTime);

        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::PENDING) continue;
            anyPending = true;

            const TaskInfo& tInfo = sc->getTasks().at(tId);
            Distance dist = sc->distanceBetween(robot.node, tInfo.node);
            Time tt       = dist / rInfo.navigationVelocity;
            Time arrival  = now + tt;

            // Ventana temporal inalcanzable: recargar no ayuda (avanza el tiempo)
            if (arrival > tInfo.latestStart) continue;

            double p = tInfo.successProb;
            BatteryLevel navCost   = tt * rInfo.batteryRateWhileNavigating;
            BatteryLevel expDemand = p * tInfo.averageSuccessDemand
                                   + (1.0 - p) * tInfo.averageFailDemand;
            // Cambio 5: batería para llegar Y para ejecutar
            if (navCost + expDemand > robot.batteryLevel) {
                anyBatteryBlocked = true;
                continue;
            }

            auto itBlock  = blockingProb.find(tId);
            double pBlock = (itBlock == blockingProb.end()) ? 0.0 : itBlock->second;
            double value  = p * (1.0 - pBlock) + 1e-3 * p;

            Time wait    = std::max(0.0, tInfo.earliestStart - arrival);
            Time slack   = tInfo.latestStart - arrival; // >= 0 garantizado
            Time execExp = p * tInfo.averageSuccessTime
                         + (1.0 - p) * tInfo.averageFailTime;

            double cost    = std::max(0.1, tt + wait + execExp);
            double tau     = std::max(0.1, tt + execExp);
            double urgency = 1.0 + URGENCY_WEIGHT * (tau / (tau + slack));
            double score   = (value / cost) * urgency;

            out.push_back({Action(Action::Type::EXECUTE_TASK, tId), score});
        }
        return out;
    }

    // Política de rollout: mejor tarea según el score; RECHARGE solo si hay
    // tareas bloqueadas por batería y la batería no está llena; FINISH si no.
    Action rolloutAction(RobotID rId, const RolloutState& s,
                          const std::unordered_map<TaskID, double>& blockingProb,
                          const std::shared_ptr<const Scenario>& sc) const {
        bool anyPending, anyBatteryBlocked;
        auto scored = scoreFeasibleTasks(rId, s, blockingProb, sc,
                                         anyPending, anyBatteryBlocked);

        const Robot& robot   = s.robots.at(rId);
        const double battCap = sc->robots.at(rId).batteryCapacity;

        if (!anyPending) return Action(Action::Type::FINISH);
        if (scored.empty()) {
            if (anyBatteryBlocked && robot.batteryLevel < battCap * 0.99)
                return Action(Action::Type::RECHARGE);
            return Action(Action::Type::FINISH);
        }

        const ScoredAction* best = &scored.front();
        for (const auto& sa : scored)
            if (sa.score > best->score) best = &sa;
        return best->action;
    }

    // Acciones disponibles para el robot planificador, ordenadas por score
    // descendente (la primera no expandida será la próxima en expandirse).
    std::vector<ScoredAction> treeActions(
        const RolloutState& s,
        const std::unordered_map<TaskID, double>& blockingProb,
        const std::shared_ptr<const Scenario>& sc) const {

        bool anyPending, anyBatteryBlocked;
        auto scored = scoreFeasibleTasks(myId, s, blockingProb, sc,
                                         anyPending, anyBatteryBlocked);

        const Robot& robot   = s.robots.at(myId);
        const double battCap = sc->robots.at(myId).batteryCapacity;

        if (!anyPending) return {{Action(Action::Type::FINISH), 1.0}};
        if (scored.empty()) {
            if (anyBatteryBlocked && robot.batteryLevel < battCap * 0.99)
                return {{Action(Action::Type::RECHARGE), 1.0}};
            return {{Action(Action::Type::FINISH), 1.0}};
        }

        std::sort(scored.begin(), scored.end(),
                  [](const ScoredAction& a, const ScoredAction& b) {
                      return a.score > b.score;
                  });

        // RECHARGE con prior mínimo: el widening lo expandirá el último
        if (robot.batteryLevel < battCap * 0.99)
            scored.push_back({Action(Action::Type::RECHARGE), RECHARGE_SCORE});
        return scored;
    }

    // =========================================================================
    // Selección D-UCT filtrada por factibilidad (cambio 3)
    // =========================================================================
    // Solo considera hijos cuya acción está en `avail`. parentN es la suma de
    // visitas descontadas de esos hijos. Devuelve nullptr si ninguno aplica.
    MCTSNode* selectChildDUCT(MCTSNode* node, const std::vector<ScoredAction>& avail) {
        std::set<std::pair<int, int>> availSet;
        for (const auto& sa : avail)
            availSet.emplace(static_cast<int>(sa.action.getType()),
                             static_cast<int>(sa.action.getTarget()));

        auto childKey = [](const MCTSNode* c) {
            return std::make_pair(static_cast<int>(c->action.getType()),
                                  static_cast<int>(c->action.getTarget()));
        };

        double parentN = 0.0;
        for (auto& c : node->children) {
            c->decayTo(globalTick, gamma_);
            if (availSet.count(childKey(c.get()))) parentN += c->N_disc;
        }

        // Hijo "fresco" disponible (N_disc ≈ 0): equivalente a UCB = +inf
        for (auto& c : node->children) {
            if (availSet.count(childKey(c.get())) && c->N_disc < 1e-9)
                return c.get();
        }

        MCTSNode* best = nullptr;
        double bestU = -std::numeric_limits<double>::infinity();
        const double logParent = std::log(std::max(parentN, 1.0));
        for (auto& c : node->children) {
            if (!availSet.count(childKey(c.get()))) continue;
            double Fbar  = c->Q_disc / c->N_disc;
            double bonus = 2.0 * cExplore_ * std::sqrt(logParent / c->N_disc);
            double U     = Fbar + bonus;
            if (U > bestU) { bestU = U; best = c.get(); }
        }
        return best;
    }

    // Punto único de lectura del canal de comunicación. Con useComm_ = false
    // devuelve un mapa vacío: los vecinos no tienen plan conocido (ablación C1).
    std::map<RobotID, std::vector<TaskID>> sampleNeighbourBundles(const Observation& obs) {
        if (!useComm_) return {};
        return sampleBundles(obs.getKnownDistributions());
    }

    // =========================================================================
    // Muestrea bundles de la distribución de cada robot vecino (idéntico a V2)
    // =========================================================================
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

    // Decide la acción de un robot vecino: sigue su bundle muestreado; si se
    // agota, política marginal-aware. V4: al seguir el bundle también se exige
    // factibilidad de ventana y batería de ejecución (un vecino real corriendo
    // V4 tampoco se comprometería a una tarea inalcanzable).
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

                const Robot& robot     = s.robots.at(rId);
                const RobotInfo& rInfo = sc->robots.at(rId);
                const TaskInfo& tInfo  = sc->getTasks().at(tId);

                Distance dist = sc->distanceBetween(robot.node, tInfo.node);
                Time tt       = dist / rInfo.navigationVelocity;
                Time now      = std::max(robot.time, s.globalTime);
                if (now + tt > tInfo.latestStart) continue;

                double p = tInfo.successProb;
                BatteryLevel navCost   = tt * rInfo.batteryRateWhileNavigating;
                BatteryLevel expDemand = p * tInfo.averageSuccessDemand
                                       + (1.0 - p) * tInfo.averageFailDemand;
                if (navCost + expDemand <= robot.batteryLevel)
                    return Action(Action::Type::EXECUTE_TASK, tId);
            }
        }
        return rolloutAction(rId, s, blockingProb, sc);
    }

    // =========================================================================
    // Física del rollout (idéntica a V2)
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
        // Fidelidad con simulator.cpp::simulateTask: mientras espera a que se
        // complete la coalición, el robot se da por ocupado hasta el cierre de la
        // ventana (estimación conservadora), no hasta el initTime provisional.
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

    void processTaskEnd(TaskID tId, bool success, RolloutState& s, EventQueue& q,
                        const std::shared_ptr<const Scenario>& sc) {
        Task& task = s.tasks.at(tId);
        const TaskInfo& tInfo = sc->getTasks().at(tId);

        Time execTime  = std::max(0.0, s.globalTime - task.initTime);
        BatteryLevel avgDemand = success ? tInfo.averageSuccessDemand : tInfo.averageFailDemand;
        Time          avgTime  = success ? tInfo.averageSuccessTime   : tInfo.averageFailTime;
        // Fidelidad con simulator.cpp::endTask: si la duración media de este
        // desenlace es 0 (típico en fail_time=[0,0]), el consumo NO es 0 sino la
        // demanda completa. Antes la réplica cobraba 0 y los rollouts trataban el
        // fracaso como gratis, infravalorando su coste energético.
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
        // Espejo del simulador corregido: ASSIGNED también caduca (si sigue en
        // ASSIGNED al llegar la expiración, su TASK_START es posterior a latestStart).
        if (task.status == TaskStatus::COMPLETED  ||
            task.status == TaskStatus::FAILED      ||
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
    // Recompensa al final del rollout (cambio 6: tie-break de earliness)
    // =========================================================================
    // base  = completadas / n
    // bono  = (W_e / n) * earliness_media,  earliness = 1 - (initTime - es)/(ls - es)
    // Como W_e < 1, el bono nunca compensa una tarea menos: el orden parcial
    // "más tareas completadas" se conserva estrictamente.
    double computeReward(const RolloutState& s,
                          const std::shared_ptr<const Scenario>& sc) const {
        int nTasks = static_cast<int>(sc->getTasks().size());
        if (nTasks == 0) return 0.0;

        int completed = 0;
        double earlinessSum = 0.0;
        for (const auto& [tId, task] : s.tasks) {
            if (task.status != TaskStatus::COMPLETED) continue;
            completed++;
            const TaskInfo& tInfo = sc->getTasks().at(tId);
            double width = std::max(1e-6, tInfo.latestStart - tInfo.earliestStart);
            double e = 1.0 - (task.initTime - tInfo.earliestStart) / width;
            earlinessSum += std::clamp(e, 0.0, 1.0);
        }

        double base  = static_cast<double>(completed) / nTasks;
        double bonus = (completed > 0)
                     ? (EARLINESS_WEIGHT / nTasks) * (earlinessSum / completed)
                     : 0.0;
        return base + bonus;
    }

    // =========================================================================
    // Simulación completa parametrizada por modo (estructura de V2 con la
    // nueva fase de árbol: widening progresivo + expansión ordenada + selección
    // filtrada por factibilidad)
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
                            auto avail = treeActions(s, blockingProb, sc); // ordenadas por score desc

                            // Widening progresivo: límite de hijos según visitas
                            currentNode->decayTo(globalTick, gamma_);
                            int limit = 1 + static_cast<int>(std::floor(
                                PW_C * std::pow(std::max(currentNode->N_disc, 1.0), PW_ALPHA)));
                            limit = std::min(limit, static_cast<int>(avail.size()));

                            // Contar hijos ya expandidos que siguen disponibles y
                            // localizar la mejor acción disponible aún sin hijo
                            int availChildren = 0;
                            const ScoredAction* toExpand = nullptr;
                            for (const auto& sa : avail) {
                                if (currentNode->findChild(sa.action)) availChildren++;
                                else if (!toExpand) toExpand = &sa;
                            }

                            if (toExpand && availChildren < limit) {
                                // Expansión: el hijo no explorado de mayor score
                                currentNode->children.push_back(
                                    std::make_unique<MCTSNode>(toExpand->action, currentNode));
                                MCTSNode* newNode = currentNode->children.back().get();
                                path.push_back(newNode);
                                currentNode = newNode;
                                action  = toExpand->action;
                                inTree  = false; // a partir de aquí, rollout
                            } else {
                                MCTSNode* best = selectChildDUCT(currentNode, avail);
                                if (best) {
                                    path.push_back(best);
                                    currentNode = best;
                                    action = best->action;
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
    // Iteración MCTS (idéntica a V2)
    // =========================================================================
    void runIteration(const Observation& obs, const std::shared_ptr<const Scenario>& sc) {
        if (!root)
            root = std::make_unique<MCTSNode>(Action(Action::Type::START), nullptr);

        auto sampledBundles = sampleNeighbourBundles(obs);
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

        for (MCTSNode* node : path) {
            node->decayTo(globalTick, gamma_);
            node->N_disc += 1.0;
            node->Q_disc += backprop_value;
        }
        globalTick++;
    }

    // =========================================================================
    // Poda de hijos obsoletos del root (cambio 4)
    // =========================================================================
    // Elimina hijos cuya acción ya es imposible según la observación actual:
    //   - EXECUTE_TASK sobre tarea no PENDING (completada/cogida/caducada)
    //   - EXECUTE_TASK cuya ventana es definitivamente inalcanzable (cota
    //     inferior de llegada: now + viaje > latestStart)
    //   - RECHARGE con la batería ya llena
    //
    // La poda es DESTRUCTIVA (se lleva por delante el subárbol del hijo), así
    // que su criterio debe ser conservador: solo acciones que ya no volverán a
    // ser aplicables. La infactibilidad *temporal* (batería insuficiente ahora
    // mismo, o FINISH mientras queden tareas) NO se poda aquí: se filtra en la
    // decisión, con `mostVisitedFeasibleChild`, para no destruir información
    // que volverá a ser útil dentro de unos instantes.
    void pruneStaleRootChildren(const Observation& obs,
                                 const std::shared_ptr<const Scenario>& sc) {
        if (!root || root->children.empty()) return;

        const auto& tasks      = obs.getKnownTasks();
        const Robot& me        = obs.getMyState();
        const RobotInfo& rInfo = sc->robots.at(myId);

        Time now = obs.getCurrentTime();
        if (me.status == RobotStatus::AVAILABLE)
            now = std::max(now, me.time);

        auto isStale = [&](const std::unique_ptr<MCTSNode>& c) {
            switch (c->action.getType()) {
                case Action::Type::EXECUTE_TASK: {
                    TaskID tId = c->action.getTarget();
                    auto it = tasks.find(tId);
                    if (it == tasks.end() ||
                        it->second.status != TaskStatus::PENDING) return true;
                    const TaskInfo& tInfo = sc->getTasks().at(tId);
                    Time tt = travelTime(myId, me.node, tInfo.node, sc);
                    return (now + tt > tInfo.latestStart);
                }
                case Action::Type::RECHARGE:
                    return me.batteryLevel >= rInfo.batteryCapacity * 0.99;
                default:
                    return false;
            }
        };

        auto& ch = root->children;
        ch.erase(std::remove_if(ch.begin(), ch.end(), isStale), ch.end());
    }

    // =========================================================================
    // Política de explotación final, filtrada por factibilidad
    // =========================================================================
    // Devuelve el hijo del root con más visitas descontadas DE ENTRE los que
    // son aplicables ahora mismo, con el mismo criterio que usa la selección
    // (`treeActions`). Devuelve nullptr si ninguno lo es.
    //
    // MOTIVO (corrección del retiro prematuro): `treeActions` solo ofrece
    // FINISH en solitario cuando no queda ninguna tarea factible. Un rollout
    // que alcanza ese estado vuelca TODAS sus visitas en el hijo FINISH,
    // mientras los hijos EXECUTE_TASK se reparten las visitas de los rollouts
    // restantes entre 12-24 hermanos. Comparando N_disc en bruto, FINISH ganaba
    // la decisión aunque quedasen tareas disponibles y el robot abandonaba la
    // misión con la batería casi intacta. Los conteos de visitas solo son
    // comparables entre acciones que compiten en las mismas condiciones.
    MCTSNode* mostVisitedFeasibleChild(const Observation& obs,
                                        const std::shared_ptr<const Scenario>& sc) {
        if (!root || root->children.empty()) return nullptr;

        // La factibilidad no depende de blockingProb (solo el orden por score).
        const std::unordered_map<TaskID, double> noBlocking;
        auto avail = treeActions(makeState(obs), noBlocking, sc);

        MCTSNode* best = nullptr;
        double bestN = -1.0;
        for (auto& c : root->children) {
            bool feasible = std::any_of(avail.begin(), avail.end(),
                                        [&](const ScoredAction& sa) {
                                            return sa.action.getType()   == c->action.getType() &&
                                                   sa.action.getTarget() == c->action.getTarget();
                                        });
            if (!feasible) continue;
            c->decayTo(globalTick, gamma_);
            if (c->N_disc > bestN) { bestN = c->N_disc; best = c.get(); }
        }
        return best;
    }

    // =========================================================================
    // Extrae la distribución del árbol (cambio 7: acumula con +=)
    // =========================================================================
    Distribution extractDistribution() {
        Distribution dist;
        if (!root) return dist;

        double rootN = 0.0;
        for (auto& c : root->children) {
            c->decayTo(globalTick, gamma_);
            rootN += c->N_disc;
        }
        if (rootN < 1e-9) return dist;

        for (auto& child : root->children) {
            if (child->N_disc < 1e-9) continue;

            Bundle bundle;
            MCTSNode* node = child.get();
            while (node && node->action.getType() == Action::Type::EXECUTE_TASK) {
                bundle.push_back(node->action.getTarget());
                MCTSNode* next = node->mostVisitedChild(globalTick, gamma_);
                node = next;
            }

            double prob = child->N_disc / rootN;
            if (prob > 0.0) dist[bundle] += prob;
        }
        return dist;
    }

public:
    // gamma: factor D-UCT. useDiffReward: doble rollout f^r (las ablaciones de
    // V2 confirmaron gamma=0.999 y useDiffReward=false como mejores).
    // cExplore: constante de exploración D-UCT (el bono efectivo es 2*cExplore).
    // NOTA sobre gamma: el valor por defecto del constructor es 0.9999, que es
    //   el de la variante "dec-mcts-v4-g9999" usada en los experimentos. La
    //   variante "dec-mcts-v4" de main.cpp lo fija explícitamente a 0.999.
    //   La ablación que lo respaldaba (0.6171 vs 0.6069; C=0.35 peor que C=0.7)
    //   se hizo sobre 16 escenarios de la familia 1E, que es DETERMINISTA: no
    //   debe extrapolarse al régimen estocástico sin repetirla allí.
    // iterationsPerCall / emergencyIters: presupuesto de cómputo MCTS. Por defecto
    // 30/300 (rápido, ~8s/run). Subirlos permite estudiar si el rendimiento mejora
    // con más cómputo (cada run tarda ~linealmente más).
    // useComm: si es false, se ignoran las distribuciones comunicadas por los
    // vecinos (ablación C1; ver el comentario del miembro useComm_).
    explicit DecMCTSSolverV4(RobotID id, double gamma = 0.9999, bool useDiffReward = false,
                             double cExplore = DEFAULT_C_EXPLORE,
                             int iterationsPerCall = ITERATIONS_PER_CALL,
                             int emergencyIters = EMERGENCY_ITERS,
                             bool useComm = true)
        : myId(id), gamma_(gamma), useDiffReward_(useDiffReward), cExplore_(cExplore),
          iterationsPerCall_(iterationsPerCall), emergencyIters_(emergencyIters),
          useComm_(useComm), rng(std::random_device{}()) {}

    // =========================================================================
    // Interfaz ISolver
    // =========================================================================

    bool requiresContinuousPlanning() const override { return true; }

    std::optional<Distribution> performBackgroundPlanning(
        const Observation& obs,
        const std::shared_ptr<const Scenario>& sc) override {

        pruneStaleRootChildren(obs, sc);

        for (int i = 0; i < iterationsPerCall_; ++i)
            runIteration(obs, sc);

        return extractDistribution();
    }

    Action decideNextAction(
        const Observation& obs,
        const std::shared_ptr<const Scenario>& sc) override {

        pruneStaleRootChildren(obs, sc);

        if (!root || root->children.empty()) {
            if (!root)
                root = std::make_unique<MCTSNode>(Action(Action::Type::START), nullptr);
            for (int i = 0; i < emergencyIters_; ++i)
                runIteration(obs, sc);
            pruneStaleRootChildren(obs, sc);
        }

        // Política robusta: hijo con más visitas descontadas de entre los que
        // son aplicables ahora mismo (ver mostVisitedFeasibleChild)
        MCTSNode* bestChild = mostVisitedFeasibleChild(obs, sc);

        Action chosen = Action(Action::Type::IDLE); // valor por defecto, no debería usarse
        if (bestChild) {
            chosen = bestChild->action;
        } else {
            // Fallback: política marginal-aware con la información actual
            auto sampledBundles = sampleNeighbourBundles(obs);
            auto blockingProb   = computeBlockingProb(myId, sampledBundles, sc);
            chosen = rolloutAction(myId, makeState(obs), blockingProb, sc);
        }

        // Re-enraizamiento + consolidación de timestamps (H3 fix de V2)
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
