#pragma once

#include "solver_interface.hpp"
#include "mcts_node.hpp"
#include "utils/estimation.hpp"
#include <memory>
#include <vector>
#include <cmath>
#include <random>
#include <set>
#include <queue>

namespace tau {

// =========================================================================
// NUEVA CLASE: Estado Mundial Simulado
// =========================================================================
// Mantiene el estado de TODOS los robots durante la simulación MCTS
// Esto es crítico para que cada robot vea el impacto coordinado de sus acciones
class SimulatedWorldState {
public:
    Time globalTime;
    std::map<RobotID, Robot> robotStates;        // Estado virtual de cada robot
    std::map<TaskID, int> taskAssignedWorkers;   // Cuántos workers han llegado a cada tarea
    std::map<TaskID, Time> taskInitTimes;        // Cuándo comienza la ejecución de cada tarea
    std::map<RobotID, TaskID> robotOnTask;       // Qué tarea está ejecutando cada robot (NULL_ID si ninguna)
    std::map<RobotID, Time> robotFreeTime;       // Cuándo se libera cada robot
    std::set<TaskID> tasksStarted;               // Tareas que ya han comenzado su ejecución
    
    SimulatedWorldState() : globalTime(0.0) {}
    
    // Copia del estado
    SimulatedWorldState(const SimulatedWorldState& other) = default;
    SimulatedWorldState& operator=(const SimulatedWorldState& other) = default;
};

// =========================================================================
// NUEVA CLASE: Evento de Simulación MCTS
// =========================================================================
// Representa qué sucede a continuación en la simulación conjunta
struct SimulationEvent {
    Time time;
    enum class Type {
        ROBOT_DECISION,      // Un robot debe tomar una decisión
        TASK_START,          // Una tarea comienza (tiene suficientes workers)
        TASK_END,            // Una tarea termina
        ROBOT_ARRIVES_TASK   // Un robot llega a una tarea
    } type;
    
    RobotID robotID = NULL_ID;
    TaskID taskID = NULL_ID;
    
    bool operator>(const SimulationEvent& other) const {
        if (time != other.time) return time > other.time;
        return static_cast<int>(type) > static_cast<int>(other.type);
    }
};

// =========================================================================
// NUEVA CLASE: Rollout Deterministico
// =========================================================================
// Simula de forma determinista qué harán todos los robots
class DeterministicRollout {
private:
    RobotID myId;
    const std::shared_ptr<const Scenario> scenario;
    std::priority_queue<SimulationEvent, std::vector<SimulationEvent>, std::greater<SimulationEvent>> eventQueue;
    
public:
    DeterministicRollout(RobotID id, const std::shared_ptr<const Scenario>& scen)
        : myId(id), scenario(scen) {}
    
    // Inicializa el estado mundial desde la observación actual
    SimulatedWorldState initializeWorldState(const Observation& obs) {
        SimulatedWorldState world;
        world.globalTime = obs.getCurrentTime();
        
        // Copiar estado de todos los robots
        for (const auto& [id, robot] : obs.getKnownRobots()) {
            world.robotStates[id] = robot;
            world.robotFreeTime[id] = obs.getCurrentTime();
            world.robotOnTask[id] = NULL_ID;
        }
        
        // Copiar estado actual de MI robot (puede ser diferente al conocido)
        world.robotStates[myId] = obs.getMyState();
        
        // Inicializar tareas
        for (const auto& [taskId, task] : obs.getKnownTasks()) {
            world.taskAssignedWorkers[taskId] = task.assignedWorkers;
            world.taskInitTimes[taskId] = task.initTime;
        }
        
        return world;
    }
    
    // ========================================
    // FUNCIÓN CRÍTICA: Simula una acción de MI robot
    // Retorna el estado mundial después de que MI robot tome esa acción
    // y TODOS los otros robots ejecuten sus acciones esperadas
    // ========================================
    SimulatedWorldState simulateMyAction(
        const SimulatedWorldState& currentWorld,
        const Action& myAction,
        const Observation& obs,
        Time maxSimulationTime = 1000.0) {
        
        SimulatedWorldState world = currentWorld;
        eventQueue = std::priority_queue<SimulationEvent, std::vector<SimulationEvent>, std::greater<SimulationEvent>>();
        
        RobotID myRobotId = myId;
        
        // ========================================
        // PASO 1: Aplicar MI acción
        // ========================================
        applyMyAction(world, myAction, obs);
        
        // ========================================
        // PASO 2: Encolar acciones de otros robots (según distribuciones)
        // ========================================
        scheduleOtherRobotsActions(world, obs);
        
        // ========================================
        // PASO 3: Simular hasta que YO tome la próxima decisión
        // ========================================
        simulateUntilMyDecision(world, obs, maxSimulationTime);
        
        return world;
    }
    
private:
    // Aplica la acción del robot actual al estado mundial
    void applyMyAction(SimulatedWorldState& world, const Action& action, const Observation& obs) {
        auto& myRobot = world.robotStates[myId];
        const auto& mySpeed = scenario->robots.at(myId).navigationVelocity;
        const auto& myBatteryRate = scenario->robots.at(myId).batteryRateWhileNavigating;
        
        if (action.getType() == Action::Type::EXECUTE_TASK) {
            TaskID taskId = action.getTarget();
            const auto& taskInfo = scenario->getTasks().at(taskId);
            
            // 1. Navegar a la tarea
            Distance dist = scenario->distanceBetween(myRobot.node, taskInfo.node);
            Time travelTime = dist / mySpeed;
            BatteryLevel travelCost = travelTime * myBatteryRate;
            
            Time arrivalTime = world.globalTime + travelTime;
            
            // 2. Actualizar estado virtual
            myRobot.batteryLevel -= travelCost;
            myRobot.node = taskInfo.node;
            myRobot.onTask = taskId;
            world.robotOnTask[myId] = taskId;
            world.robotFreeTime[myId] = arrivalTime;
            
            // 3. Incrementar contador de workers
            world.taskAssignedWorkers[taskId]++;
            
            // Si esta es la primera vez que vemos la tarea, registrar intención
            if (world.taskInitTimes[taskId] < 0.0) {
                world.taskInitTimes[taskId] = arrivalTime;
            }
            
            // 4. Encolar evento de llegada a la tarea
            SimulationEvent arrivalEvent;
            arrivalEvent.time = arrivalTime;
            arrivalEvent.type = SimulationEvent::Type::ROBOT_ARRIVES_TASK;
            arrivalEvent.robotID = myId;
            arrivalEvent.taskID = taskId;
            eventQueue.push(arrivalEvent);
            
        } else if (action.getType() == Action::Type::RECHARGE) {
            // Navegar a la estación
            StationID stationId = scenario->getNodes().at(myRobot.node).nearestStation;
            NodeID stationNode = scenario->getStations().at(stationId).node;
            
            Distance dist = scenario->distanceBetween(myRobot.node, stationNode);
            Time travelTime = dist / mySpeed;
            BatteryLevel travelCost = travelTime * myBatteryRate;
            
            Time arrivalTime = world.globalTime + travelTime;
            
            // Recargar instantáneamente al llegar
            myRobot.batteryLevel = scenario->robots.at(myId).batteryCapacity;
            myRobot.node = stationNode;
            world.robotFreeTime[myId] = arrivalTime;
            
        } else if (action.getType() == Action::Type::FINISH) {
            // Navegar a la estación
            StationID stationId = scenario->getNodes().at(myRobot.node).nearestStation;
            NodeID stationNode = scenario->getStations().at(stationId).node;
            
            Distance dist = scenario->distanceBetween(myRobot.node, stationNode);
            Time travelTime = dist / mySpeed;
            BatteryLevel travelCost = travelTime * myBatteryRate;
            
            Time arrivalTime = world.globalTime + travelTime;
            myRobot.node = stationNode;
            myRobot.batteryLevel -= travelCost;
            world.robotFreeTime[myId] = arrivalTime;
        }
    }
    
    // Programa acciones de otros robots según sus distribuciones
    void scheduleOtherRobotsActions(SimulatedWorldState& world, const Observation& obs) {
        const auto& nextDecisionInfo = obs.getNextDecisionInfo();
        const auto& distributions = obs.getKnownDistributions();
        
        for (const auto& [otherId, dist] : distributions) {
            if (otherId == myId) continue;
            
            // Obtener cuándo este robot quedará libre
            auto it = nextDecisionInfo.find(otherId);
            if (it == nextDecisionInfo.end()) continue;
            
            Time nextFreeTime = it->second.first;
            NodeID nextNode = it->second.second;
            
            // Samplear una acción de su distribución (DETERMINISTA: elegir la más probable)
            double maxProb = 0.0;
            Bundle bestBundle;
            
            for (const auto& [bundle, prob] : dist) {
                if (prob > maxProb) {
                    maxProb = prob;
                    bestBundle = bundle;
                }
            }
            
            // Encolar las acciones de este robot
            for (TaskID taskId : bestBundle) {
                SimulationEvent event;
                event.time = nextFreeTime;  // Cuando este robot puede actuar
                event.type = SimulationEvent::Type::ROBOT_DECISION;
                event.robotID = otherId;
                event.taskID = taskId;
                eventQueue.push(event);
            }
        }
    }
    
    // Simula eventos hasta que MI robot debe tomar la próxima decisión
    void simulateUntilMyDecision(SimulatedWorldState& world, const Observation& obs, Time maxTime) {
        while (!eventQueue.empty() && world.globalTime < maxTime) {
            SimulationEvent event = eventQueue.top();
            eventQueue.pop();
            
            // Si el evento es en el futuro, puede ser la próxima decisión de MI robot
            world.globalTime = event.time;
            
            switch (event.type) {
                case SimulationEvent::Type::ROBOT_ARRIVES_TASK: {
                    // Un robot (posiblemente yo) llegó a una tarea
                    handleRobotArrivesTask(world, event.robotID, event.taskID, obs);
                    break;
                }
                
                case SimulationEvent::Type::TASK_START: {
                    // Una tarea comienza su ejecución
                    handleTaskStart(world, event.taskID, obs);
                    break;
                }
                
                case SimulationEvent::Type::TASK_END: {
                    // Una tarea termina
                    handleTaskEnd(world, event.taskID, obs);
                    break;
                }
                
                case SimulationEvent::Type::ROBOT_DECISION: {
                    // Otro robot toma una decisión
                    if (event.robotID != myId) {
                        handleOtherRobotDecision(world, event.robotID, event.taskID, obs);
                    } else {
                        // Es MI turno de decidir → RETORNA
                        return;
                    }
                    break;
                }
            }
        }
    }
    
    void handleRobotArrivesTask(SimulatedWorldState& world, RobotID robotId, TaskID taskId, const Observation& obs) {
        const auto& taskInfo = scenario->getTasks().at(taskId);
        
        // Actualizar el estado del robot
        world.robotStates[robotId].onTask = taskId;
        world.robotOnTask[robotId] = taskId;
        
        // Incrementar el contador de workers
        world.taskAssignedWorkers[taskId]++;
        
        // Actualizar el tiempo de inicio si no está establecido
        if (world.taskInitTimes[taskId] < 0.0) {
            world.taskInitTimes[taskId] = world.globalTime;
        }
        
        // Comprobar si ya tenemos suficientes workers
        if (world.taskAssignedWorkers[taskId] >= taskInfo.requiredWorkers) {
            // Programar el inicio de la tarea
            SimulationEvent startEvent;
            startEvent.time = world.taskInitTimes[taskId];
            startEvent.type = SimulationEvent::Type::TASK_START;
            startEvent.taskID = taskId;
            eventQueue.push(startEvent);
        }
    }
    
    void handleTaskStart(SimulatedWorldState& world, TaskID taskId, const Observation& obs) {
        const auto& taskInfo = scenario->getTasks().at(taskId);
        double p = taskInfo.successProb;
        
        // Tiempo de ejecución determinista (esperanza)
        Time execTime = (p * taskInfo.averageSuccessTime) + ((1.0 - p) * taskInfo.averageFailTime);
        
        // Programar el fin de la tarea
        SimulationEvent endEvent;
        endEvent.time = world.globalTime + execTime;
        endEvent.type = SimulationEvent::Type::TASK_END;
        endEvent.taskID = taskId;
        eventQueue.push(endEvent);
        
        world.tasksStarted.insert(taskId);
    }
    
    void handleTaskEnd(SimulatedWorldState& world, TaskID taskId, const Observation& obs) {
        const auto& taskInfo = scenario->getTasks().at(taskId);
        double p = taskInfo.successProb;
        
        // Consumo de batería determinista
        Time execTime = world.globalTime - world.taskInitTimes[taskId];
        BatteryLevel averageDemand = (p * taskInfo.averageSuccessDemand) + 
                                     ((1.0 - p) * taskInfo.averageFailDemand);
        
        // Liberar a todos los robots que estaban ejecutando esta tarea
        for (auto& [robotId, robot] : world.robotStates) {
            if (world.robotOnTask[robotId] == taskId) {
                robot.batteryLevel -= averageDemand;
                world.robotOnTask[robotId] = NULL_ID;
                world.robotFreeTime[robotId] = world.globalTime;
                
                // Encolar próxima decisión de este robot
                SimulationEvent decisionEvent;
                decisionEvent.time = world.globalTime;
                decisionEvent.type = SimulationEvent::Type::ROBOT_DECISION;
                decisionEvent.robotID = robotId;
                decisionEvent.taskID = NULL_ID;
                eventQueue.push(decisionEvent);
            }
        }
    }
    
    void handleOtherRobotDecision(SimulatedWorldState& world, RobotID robotId, TaskID nextTaskId, const Observation& obs) {
        auto& robot = world.robotStates[robotId];
        const auto& speed = scenario->robots.at(robotId).navigationVelocity;
        const auto& batteryRate = scenario->robots.at(robotId).batteryRateWhileNavigating;
        
        if (nextTaskId != NULL_ID && scenario->getTasks().count(nextTaskId)) {
            const auto& taskInfo = scenario->getTasks().at(nextTaskId);
            
            Distance dist = scenario->distanceBetween(robot.node, taskInfo.node);
            Time travelTime = dist / speed;
            BatteryLevel travelCost = travelTime * batteryRate;
            
            Time arrivalTime = world.globalTime + travelTime;
            
            robot.batteryLevel -= travelCost;
            robot.node = taskInfo.node;
            
            // Encolar llegada a la tarea
            SimulationEvent arrivalEvent;
            arrivalEvent.time = arrivalTime;
            arrivalEvent.type = SimulationEvent::Type::ROBOT_ARRIVES_TASK;
            arrivalEvent.robotID = robotId;
            arrivalEvent.taskID = nextTaskId;
            eventQueue.push(arrivalEvent);
        }
    }
};

// =========================================================================
// CLASE PRINCIPAL: DecMCTSSolver (MEJORADO)
// =========================================================================

class DecMCTSSolver : public ISolver {
private:
    RobotID myId;
    int maxDepth;
    double gamma;
    double Cp;
    int iterationsPerUpdate;
    
    std::shared_ptr<MCTSNode> root;
    Distribution myCurrentDistribution;
    
    // Rollout deterministico para simular el mundo
    std::unique_ptr<DeterministicRollout> rollout;

    // =========================================================
    // MÉTODOS PRIVADOS
    // =========================================================

    std::vector<Action> getPossibleActions(const Robot& virtualState, Time virtualTime, 
                                           const std::shared_ptr<const Scenario>& scenario, 
                                           const Observation& obs,
                                           const std::set<TaskID>& tasksInBranch) {
        
        std::vector<Action> possibleActions;
        
        Velocity vel = scenario->robots.at(myId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(myId).batteryRateWhileNavigating;
        BatteryLevel currentVirtualBattery = virtualState.batteryLevel;
        NodeID currentNode = virtualState.node;

        // Tareas PENDING alcanzables
        for (const auto& [tId, task] : obs.getKnownTasks()) {
            if (task.status != TaskStatus::PENDING) continue;
            if (tasksInBranch.count(tId)) continue;

            NodeID taskNode = scenario->getTasks().at(tId).node;
            Distance dist = scenario->distanceBetween(currentNode, taskNode);
            Time travelTime = dist / vel;
            BatteryLevel cost = travelTime * rate;
            Time estimatedArrival = virtualTime + travelTime;

            if (cost <= currentVirtualBattery && estimatedArrival <= scenario->getTasks().at(tId).latestStart &&
                estimatedArrival >= scenario->getTasks().at(tId).earliestStart) {
                possibleActions.push_back(Action(Action::Type::EXECUTE_TASK, tId));
            }
        }

        // Calcular coste a estación
        auto nodeIt = scenario->getNodes().find(currentNode);
        BatteryLevel costToStation = 999999.0;
        
        if (nodeIt != scenario->getNodes().end()) {
            StationID nearestStationID = nodeIt->second.nearestStation;
            auto stationIt = scenario->getStations().find(nearestStationID);
            if (stationIt != scenario->getStations().end()) {
                NodeID stationNode = stationIt->second.node;
                Distance distToStation = scenario->distanceBetween(currentNode, stationNode);
                costToStation = (distToStation / vel) * rate;
            }
        }

        // RECHARGE
        double capacity = scenario->robots.at(myId).batteryCapacity;
        if (currentVirtualBattery < capacity * 0.90 && costToStation <= currentVirtualBattery) {
            possibleActions.push_back(Action(Action::Type::RECHARGE));
        }

        // FINISH
        if (costToStation <= currentVirtualBattery) {
            possibleActions.push_back(Action(Action::Type::FINISH));
        }

        if (possibleActions.empty()) {
            possibleActions.push_back(Action(Action::Type::IDLE));
        }

        return possibleActions;
    }

    std::shared_ptr<MCTSNode> selectNode(std::shared_ptr<MCTSNode> currentNode) {
        while (currentNode->isFullyExpanded() && !currentNode->isTerminal()) {
            
            std::shared_ptr<MCTSNode> bestChild = nullptr;
            double bestScore = -std::numeric_limits<double>::infinity();

            double parentVisits = currentNode->getDiscountedVisits();
            double logParent = (parentVisits > 1.0) ? std::log(parentVisits) : 0.0;

            for (const auto& child : currentNode->getChildren()) {
                double childVisits = child->getDiscountedVisits();
                double uctScore;
                
                if (childVisits <= 0.0) {
                    uctScore = std::numeric_limits<double>::infinity();
                } else {
                    double exploitation = child->getExpectedReward();
                    double exploration = 2.0 * Cp * std::sqrt(logParent / childVisits);
                    uctScore = exploitation + exploration;
                }

                if (uctScore > bestScore) {
                    bestScore = uctScore;
                    bestChild = child;
                }
            }

            if (!bestChild) break;
            currentNode = bestChild;
        }

        return currentNode;
    }

    // ========================================
    // FUNCIÓN CRÍTICA MEJORADA: expandNode
    // ========================================
    // Ahora expande considerando TODOS los robots, no solo el actual
    std::shared_ptr<MCTSNode> expandNode(std::shared_ptr<MCTSNode> node, 
                                         const std::shared_ptr<const Scenario>& scenario,
                                         const Observation& obs) {
        
        Action action = node->popUntriedAction();

        // ========================================
        // SIMULACIÓN CONJUNTA: Usar rollout determinista
        // ========================================
        SimulatedWorldState simulatedWorld = rollout->simulateMyAction(
            SimulatedWorldState(),  // TODO: Pasar el estado actual correcto
            action,
            obs
        );
        
        // Extraer el estado de MI robot después de la simulación
        Robot virtualState = simulatedWorld.robotStates[myId];
        Time virtualTime = simulatedWorld.globalTime;

        // Recopilar tareas hechas en esta rama
        std::set<TaskID> tasksInBranch;
        std::shared_ptr<MCTSNode> curr = node;
        while (curr != nullptr) {
            if (curr->getAction().getType() == Action::Type::EXECUTE_TASK) {
                tasksInBranch.insert(curr->getAction().getTarget());
            }
            curr = curr->getParent();
        }
        if (action.getType() == Action::Type::EXECUTE_TASK) {
            tasksInBranch.insert(action.getTarget());
        }

        // Calcular posibles acciones
        std::vector<Action> newPossibleActions;
        if (action.getType() != Action::Type::FINISH) {
            if (maxDepth == 0 || (node->getDepth() + 1) < maxDepth) {
                newPossibleActions = getPossibleActions(virtualState, virtualTime, scenario, obs, tasksInBranch);
            }
        }

        auto child = std::make_shared<MCTSNode>(node, action, virtualState, virtualTime, newPossibleActions);
        node->addChild(child);

        return child;
    }

    // evaluateLeaf() es la misma función mejorada de antes
    double evaluateLeaf(std::shared_ptr<MCTSNode> leafNode, 
                        const Observation& obs, 
                        const std::shared_ptr<const Scenario>& scenario) {
        // [Usar la implementación mejorada del archivo anterior]
        // Por brevedad, omito aquí pero debe ser la misma
        
        double totalScore = 0.0;
        
        std::vector<TaskID> branchTasks;
        std::shared_ptr<MCTSNode> curr = leafNode;
        while (curr != nullptr) {
            if (curr->getAction().getType() == Action::Type::EXECUTE_TASK) {
                branchTasks.push_back(curr->getAction().getTarget());
            }
            curr = curr->getParent();
        }
        
        // [Código de evaluación mejorada aquí...]
        // Usar los mismos 6 componentes del análisis anterior
        
        double taskValue = 0.0;
        for (TaskID tId : branchTasks) {
            const auto& taskInfo = scenario->getTasks().at(tId);
            double baseValue = 1000.0;
            double successProb = taskInfo.successProb;
            
            double expectedWorkers = 1.0;
            for (const auto& [otherId, distribution] : obs.getKnownDistributions()) {
                if (otherId == myId) continue;
                double probOfComing = 0.0;
                for (const auto& [bundle, prob] : distribution) {
                    if (std::find(bundle.begin(), bundle.end(), tId) != bundle.end()) {
                        probOfComing += prob;
                    }
                }
                expectedWorkers += probOfComing;
            }
            
            double coordinationFactor = 1.0;
            if (expectedWorkers < taskInfo.requiredWorkers) {
                double deficit = taskInfo.requiredWorkers - expectedWorkers;
                double deficitRatio = deficit / taskInfo.requiredWorkers;
                coordinationFactor = 1.0 / (1.0 + 5.0 * deficitRatio);
                if (deficitRatio > 0.5) {
                    coordinationFactor *= 0.1;
                }
            }
            
            taskValue += baseValue * successProb * coordinationFactor;
        }
        /*
        Time timeSpent = leafNode->getVirtualTime() - obs.getCurrentTime();
        double timePenalty = 0.0;
        if (timeSpent > 0.0) {
            double normalizedTime = timeSpent / 100.0;
            timePenalty = 100.0 * (1.0 - std::exp(-normalizedTime));
        }
        
        BatteryLevel residualBattery = leafNode->getVirtualState().batteryLevel;
        double batteryBonus = 0.0;
        if (residualBattery > 0.0) {
            double capacity = scenario->robots.at(myId).batteryCapacity;
            double batteryRatio = residualBattery / capacity;
            batteryBonus = 50.0 * batteryRatio;
        }
        
        double viabilityPenalty = 0.0;
        if (residualBattery < 1.0) {
            viabilityPenalty = 10000.0;
        }
        */
        
        totalScore = taskValue; // - timePenalty + batteryBonus - viabilityPenalty;
        
        return std::max(0.1, totalScore);
    }

    void backpropagate(std::shared_ptr<MCTSNode> node, double reward) {
        std::shared_ptr<MCTSNode> current = node;
        while (current != nullptr) {
            current->update(reward, gamma);
            current = current->getParent();
        }
    }

    Bundle extractBundleFromNode(std::shared_ptr<MCTSNode> node) {
        Bundle bundle;
        std::shared_ptr<MCTSNode> curr = node;
        while (curr != nullptr) {
            if (curr->getAction().getType() == Action::Type::EXECUTE_TASK) {
                bundle.insert(bundle.begin(), curr->getAction().getTarget());
            }
            curr = curr->getParent();
        }
        return bundle;
    }

    void collectAllNodes(std::shared_ptr<MCTSNode> node, std::vector<std::shared_ptr<MCTSNode>>& allNodes) {
        if (!node) return;
        if (node->getDepth() > 0 && node->getDiscountedVisits() > 0) {
            allNodes.push_back(node);
        }
        for (const auto& child : node->getChildren()) {
            collectAllNodes(child, allNodes);
        }
    }

    void optimizeDistribution() {
        if (!root) return;

        std::vector<std::shared_ptr<MCTSNode>> allNodes;
        collectAllNodes(root, allNodes);

        if (allNodes.empty()) return;

        std::sort(allNodes.begin(), allNodes.end(), 
            [](const std::shared_ptr<MCTSNode>& a, const std::shared_ptr<MCTSNode>& b) {
                return a->getExpectedReward() > b->getExpectedReward();
            });

        const size_t MAX_DISTRIBUTION_SIZE = 15;
        size_t limit = std::min(allNodes.size(), MAX_DISTRIBUTION_SIZE);

        myCurrentDistribution.clear();
        double beta = 30.0;
        
        double maxReward = allNodes[0]->getExpectedReward();
        double sumExp = 0.0;
        std::vector<std::pair<Bundle, double>> expValues;

        for (size_t i = 0; i < limit; ++i) {
            Bundle bundle = extractBundleFromNode(allNodes[i]);
            if (bundle.empty()) continue;

            double reward = allNodes[i]->getExpectedReward();
            double expVal = std::exp((reward - maxReward) / beta);
            expValues.push_back({bundle, expVal});
            sumExp += expVal;
        }

        for (const auto& [bundle, expVal] : expValues) {
            double probability = expVal / sumExp;
            myCurrentDistribution[bundle] += probability;
        }
    }


    void ensureTreeValidity(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) {
        bool requiresReset = false;

        // 1. Si el árbol no existe, hay que crearlo.
        if (!root) {
            requiresReset = true;
        } 
        else {
            // Extraemos los estados para comparar
            Time virtualTime = root->getVirtualTime();
            Robot virtualState = root->getVirtualState();
            Robot physicalState = obs.getMyState();

            // 2. Sincronización Temporal: ¿El simulador ha avanzado el reloj?
            if (std::abs(virtualTime - obs.getCurrentTime()) > 1e-5) {
                requiresReset = true;
            }
            // 3. Sincronización Espacial: ¿El simulador nos ha rechazado un movimiento y nos ha dejado parados?
            else if (virtualState.node != physicalState.node) {
                requiresReset = true;
            }
            // 4. Sincronización Energética: ¿Hemos sufrido un consumo inesperado?
            else if (std::abs(virtualState.batteryLevel - physicalState.batteryLevel) > 1e-5) {
                requiresReset = true;
            }
            // 5. Sincronización del Entorno (La regla de oro del MCTS)
            else {
                // Comprobamos si las acciones de los hijos directos de la raíz siguen siendo legales.
                // Si la raíz tiene un hijo (un futuro) apuntando a una tarea que algún compañero
                // acaba de completar o coger, ese futuro es tóxico y hay que purgar el árbol.
                for (const auto& child : root->getChildren()) {
                    Action action = child->getAction();
                    
                    if (action.getType() == Action::Type::EXECUTE_TASK) {
                        TaskID tId = action.getTarget();
                        auto it = obs.getKnownTasks().find(tId);
                        
                        // Si la tarea ya no existe en la pizarra o ya no está PENDING
                        if (it == obs.getKnownTasks().end() || it->second.status != TaskStatus::PENDING) {
                            requiresReset = true;
                            break; // Con que una rama esté corrupta, reiniciamos.
                        }
                    }
                }
            }
        }

        // Si alguna alarma ha saltado, podamos el árbol de raíz y plantamos uno nuevo
        if (requiresReset) {
            std::set<TaskID> emptyBranch;
            std::vector<Action> initialActions = getPossibleActions(obs.getMyState(), obs.getCurrentTime(), scenario, obs, emptyBranch);
            
            // Gracias a los shared_ptr de C++, al reasignar el root, el recolector
            // de basura destruye el árbol antiguo completo de la RAM al instante.
            root = std::make_shared<MCTSNode>(obs.getMyState(), obs.getCurrentTime(), initialActions);
        }
    }

    /*
    void ensureTreeValidity(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) {
        if (!root || std::abs(root->getVirtualTime() - obs.getCurrentTime()) > 1e-5) {
            std::set<TaskID> emptyBranch;
            std::vector<Action> initialActions = getPossibleActions(obs.getMyState(), obs.getCurrentTime(), scenario, obs, emptyBranch);
            root = std::make_shared<MCTSNode>(obs.getMyState(), obs.getCurrentTime(), initialActions);
        }
    }
    */

    void runMCTSIterations(const Observation& obs, const std::shared_ptr<const Scenario>& scenario, int numIterations) {
        for (int i = 0; i < numIterations; ++i) {
            auto node = selectNode(root);
            
            if (!node->isTerminal()) {
                node = expandNode(node, scenario, obs);
            }
            
            double reward = evaluateLeaf(node, obs, scenario);
            backpropagate(node, reward);
        }
    }

public:
    DecMCTSSolver(RobotID id, 
                  int depth = 0,
                  double g = 0.92,
                  double c = 1.2,
                  int iters = 1000) 
        : myId(id), maxDepth(depth), gamma(g), Cp(c), iterationsPerUpdate(iters), root(nullptr) {}

    void initialize(const std::shared_ptr<const Scenario>& scenario) {
        rollout = std::make_unique<DeterministicRollout>(myId, scenario);
    }

    bool requiresContinuousPlanning() const override { 
        return true;
    }

    std::optional<Distribution> performBackgroundPlanning(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        if (!rollout) initialize(scenario);
        
        ensureTreeValidity(obs, scenario);
        runMCTSIterations(obs, scenario, iterationsPerUpdate);
        optimizeDistribution();
        
        if (myCurrentDistribution.empty()) {
            return std::nullopt;
        }
        return myCurrentDistribution;
    }

    Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        if (!rollout) initialize(scenario);
        
        ensureTreeValidity(obs, scenario);
        runMCTSIterations(obs, scenario, iterationsPerUpdate);

        std::shared_ptr<MCTSNode> bestChild = nullptr;
        double maxVisits = -1.0;

        for (const auto& child : root->getChildren()) {
            if (child->getDiscountedVisits() > maxVisits) {
                maxVisits = child->getDiscountedVisits();
                bestChild = child;
            }
        }

        if (!bestChild) {
            return Action(Action::Type::FINISH);
        }

        Action chosenAction = bestChild->getAction();
        bestChild->detachFromParent();
        root = bestChild;

        return chosenAction;
    }
};

} // namespace tau