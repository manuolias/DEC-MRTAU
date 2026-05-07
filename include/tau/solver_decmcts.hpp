#pragma once

#include "solver_interface.hpp"
#include "mcts_node.hpp"
#include "utils/estimation.hpp"
#include <memory>
#include <vector>
#include <cmath>
#include <random>
#include <set>

namespace tau {

class DecMCTSSolver : public ISolver {
private:
    RobotID myId;

    // --- Hiperparámetros del algoritmo (MEJORADOS) ---
    int maxDepth;               // ↑ Aumentado a 5 para horizonte más profundo
    double gamma;               // ↓ Reducido a 0.92 para menos olvido
    double Cp;                  // ↓ Reducido a 1.2 para más explotación
    int iterationsPerUpdate;    // ↑ Aumentado a 150 para mejor calidad

    // El árbol de búsqueda se mantiene entre llamadas
    std::shared_ptr<MCTSNode> root;
    
    // Distribución actual de mis propios planes
    Distribution myCurrentDistribution;

    // =========================================================
    // BLOQUE 1: LÓGICA INTERNA DEL MCTS
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

        // 1. Tareas PENDING alcanzables
        for (const auto& [tId, task] : obs.getKnownTasks()) {
            if (task.status != TaskStatus::PENDING) continue;
            if (tasksInBranch.count(tId)) continue;

            NodeID taskNode = scenario->getTasks().at(tId).node;
            Distance dist = scenario->distanceBetween(currentNode, taskNode);
            Time travelTime = dist / vel;
            BatteryLevel cost = travelTime * rate;
            Time estimatedArrival = virtualTime + travelTime;

            if (cost <= currentVirtualBattery && estimatedArrival <= scenario->getTasks().at(tId).latestStart) {
                possibleActions.push_back(Action(Action::Type::EXECUTE_TASK, tId));
            }
        }

        // 2. Calcular coste a estación
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

        // 3. RECHARGE (MEJORADO: Recargamos antes, a 90% en lugar de 95%)
        double capacity = scenario->robots.at(myId).batteryCapacity;
        if (currentVirtualBattery < capacity * 0.90 && costToStation <= currentVirtualBattery) {
            possibleActions.push_back(Action(Action::Type::RECHARGE));
        }

        // 4. FINISH (si podemos llegar a la estación vivos)
        if (costToStation <= currentVirtualBattery) {
            possibleActions.push_back(Action(Action::Type::FINISH));
        }

        // 5. Si no hay acciones, añadir IDLE
        if (possibleActions.empty()) {
            possibleActions.push_back(Action(Action::Type::IDLE));
        }

        return possibleActions;
    }

    // Fase 1: Selección (D-UCT)
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

    // Fase 2: Expansión
    std::shared_ptr<MCTSNode> expandNode(std::shared_ptr<MCTSNode> node, 
                                         const std::shared_ptr<const Scenario>& scenario,
                                         const Observation& obs) {
        
        Action action = node->popUntriedAction();

        Robot virtualState = node->getVirtualState();
        Time virtualTime = node->getVirtualTime();

        Velocity vel = scenario->robots.at(myId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(myId).batteryRateWhileNavigating;
        NodeID currentNode = virtualState.node;

        // Simulación Determinista
        if (action.getType() == Action::Type::EXECUTE_TASK) {
            TaskID tId = action.getTarget();
            const auto& taskInfo = scenario->getTasks().at(tId);
            
            Distance dist = scenario->distanceBetween(currentNode, taskInfo.node);
            Time travelTime = dist / vel;
            BatteryLevel travelCost = travelTime * rate;

            double p = taskInfo.successProb;
            Time expectedExecTime = (p * taskInfo.averageSuccessTime) + ((1.0 - p) * taskInfo.averageFailTime);
            BatteryLevel expectedExecCost = (p * taskInfo.averageSuccessDemand) + ((1.0 - p) * taskInfo.averageFailDemand);

            Time arrivalTime = virtualTime + travelTime;
            Time startTime = std::max(arrivalTime, taskInfo.earliestStart);

            virtualTime = startTime + expectedExecTime;
            virtualState.batteryLevel -= (travelCost + expectedExecCost);
            virtualState.node = taskInfo.node;

        } else if (action.getType() == Action::Type::RECHARGE || action.getType() == Action::Type::FINISH) {
            StationID nearestStationID = scenario->getNodes().at(currentNode).nearestStation;
            NodeID stationNode = scenario->getStations().at(nearestStationID).node;
            
            Distance dist = scenario->distanceBetween(currentNode, stationNode);
            Time travelTime = dist / vel;
            BatteryLevel travelCost = travelTime * rate;

            virtualTime += travelTime;
            virtualState.node = stationNode;
            
            if (action.getType() == Action::Type::RECHARGE) {
                virtualState.batteryLevel = scenario->robots.at(myId).batteryCapacity;
            } else {
                virtualState.batteryLevel -= travelCost;
            }
        } else if (action.getType() == Action::Type::START || action.getType() == Action::Type::IDLE) {
            virtualTime += 1.0;
        }

        // Recopilar tareas en la rama
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

    // Fase 3: Evaluación Determinista (MEJORADA COMPLETAMENTE)
    double evaluateLeaf(std::shared_ptr<MCTSNode> leafNode, 
                        const Observation& obs, 
                        const std::shared_ptr<const Scenario>& scenario) {
        
        double totalScore = 0.0;
        
        // 1. Recopilar tareas en esta rama
        std::vector<TaskID> branchTasks;
        std::shared_ptr<MCTSNode> curr = leafNode;
        while (curr != nullptr) {
            if (curr->getAction().getType() == Action::Type::EXECUTE_TASK) {
                branchTasks.push_back(curr->getAction().getTarget());
            }
            curr = curr->getParent();
        }
        
        // ========================================
        // COMPONENTE 1: VALOR DE TAREAS
        // ========================================
        double taskValue = 0.0;
        
        for (TaskID tId : branchTasks) {
            const auto& taskInfo = scenario->getTasks().at(tId);
            
            // Valor base (ajusta según tu escenario)
            double baseValue = 1000.0;
            
            // Probabilidad de éxito
            double successProb = taskInfo.successProb;
            
            // --- COORDINACIÓN MEJORADA ---
            double expectedWorkers = 1.0;  // Nosotros
            
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
            
            // Penalización basada en déficit de workers
            double coordinationFactor = 1.0;
            
            if (expectedWorkers < taskInfo.requiredWorkers) {
                double deficit = taskInfo.requiredWorkers - expectedWorkers;
                double deficitRatio = deficit / taskInfo.requiredWorkers;
                
                // Sigmoidea: suave pero drástica
                coordinationFactor = 1.0 / (1.0 + 5.0 * deficitRatio);
                
                // Si falta mucho personal, penalización severa
                if (deficitRatio > 0.5) {
                    coordinationFactor *= 0.1;
                }
            } else {
                // No aplicar bonus por exceso (evita redundancia)
                coordinationFactor = 1.0;
            }
            
            taskValue += baseValue * successProb * coordinationFactor;
        }
        
        // ========================================
        // COMPONENTE 2: PENALIZACIÓN TEMPORAL
        // ========================================
        Time timeSpent = leafNode->getVirtualTime() - obs.getCurrentTime();
        double timePenalty = 0.0;
        
        if (timeSpent > 0.0) {
            // Penalización no lineal
            double normalizedTime = timeSpent / 100.0;
            timePenalty = 100.0 * (1.0 - std::exp(-normalizedTime));
        }
        
        // ========================================
        // COMPONENTE 3: PENALIZACIÓN POR INEFICIENCIA ESPACIAL
        // ========================================
        double spatialPenalty = 0.0;
        if (!branchTasks.empty()) {
            double estimatedDistance = 0.0;
            
            NodeID currentNode = obs.getMyState().node;
            for (TaskID tId : branchTasks) {
                const auto& taskInfo = scenario->getTasks().at(tId);
                estimatedDistance += scenario->distanceBetween(currentNode, taskInfo.node);
                currentNode = taskInfo.node;
            }
            
            // Agregar distancia a la estación
            auto nodeIt = scenario->getNodes().find(currentNode);
            if (nodeIt != scenario->getNodes().end()) {
                StationID stationID = nodeIt->second.nearestStation;
                auto stationIt = scenario->getStations().find(stationID);
                if (stationIt != scenario->getStations().end()) {
                    estimatedDistance += scenario->distanceBetween(
                        currentNode, stationIt->second.node);
                }
            }
            
            // Penalizar baja densidad de tareas
            double taskDensity = branchTasks.size() / (estimatedDistance + 1.0);
            if (taskDensity < 0.1) {
                spatialPenalty = 100.0 * (0.1 - taskDensity);
            }
        }
        
        // ========================================
        // COMPONENTE 4: BONIFICACIÓN POR BATERÍA RESIDUAL
        // ========================================
        double batteryBonus = 0.0;
        BatteryLevel residualBattery = leafNode->getVirtualState().batteryLevel;
        
        if (residualBattery > 0.0) {
            double capacity = scenario->robots.at(myId).batteryCapacity;
            double batteryRatio = residualBattery / capacity;
            batteryBonus = 50.0 * batteryRatio;
        }
        
        // ========================================
        // COMPONENTE 5: PENALIZACIÓN POR NO VIABILIDAD
        // ========================================
        double viabilityPenalty = 0.0;
        
        if (residualBattery < 0.0) {
            // ¡Sin batería! Penalización catastrófica
            viabilityPenalty = 10000.0;
        } else if (residualBattery < scenario->robots.at(myId).batteryCapacity * 0.1) {
            // Batería crítica
            double criticalRatio = 0.1 - (residualBattery / scenario->robots.at(myId).batteryCapacity);
            viabilityPenalty = 1000.0 * criticalRatio;
        }
        
        // ========================================
        // COMPONENTE 6: PENALIZACIÓN POR TERMINAL EN IDLE
        // ========================================
        double idlePenalty = 0.0;
        if (leafNode->getAction().getType() == Action::Type::IDLE) {
            idlePenalty = 5000.0;
        }
        
        // ========================================
        // COMBINACIÓN FINAL
        // ========================================
        totalScore = 
            taskValue 
            - timePenalty 
            - spatialPenalty 
            + batteryBonus 
            - viabilityPenalty 
            - idlePenalty;
        
        // Mínimo de 0.1 para evitar problemas con log(0) en UCB
        return std::max(0.1, totalScore);
    }

    // Fase 4: Retropropagación
    void backpropagate(std::shared_ptr<MCTSNode> node, double reward) {
        std::shared_ptr<MCTSNode> current = node;
        
        while (current != nullptr) {
            current->update(reward, gamma);
            current = current->getParent();
        }
    }

    // =========================================================
    // BLOQUE 2: OPTIMIZACIÓN VARIACIONAL
    // =========================================================

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

        // MEJORADO: Aumentar a 15 en lugar de 10
        const size_t MAX_DISTRIBUTION_SIZE = 15;
        size_t limit = std::min(allNodes.size(), MAX_DISTRIBUTION_SIZE);

        myCurrentDistribution.clear();
        
        // MEJORADO: Reducir beta a 30.0 para distribuciones más concentradas
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

    // =========================================================
    // BLOQUE 3: INTERFAZ CON EL SIMULADOR
    // =========================================================

    /*
    void ensureTreeValidity(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) {
        if (!root || std::abs(root->getVirtualTime() - obs.getCurrentTime()) > 1e-5) {
            std::set<TaskID> emptyBranch;
            std::vector<Action> initialActions = getPossibleActions(obs.getMyState(), obs.getCurrentTime(), scenario, obs, emptyBranch);
            root = std::make_shared<MCTSNode>(obs.getMyState(), obs.getCurrentTime(), initialActions);
        }
    }
    */

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

public:
    // MEJORADO: Nuevos hiperparámetros
    DecMCTSSolver(RobotID id, 
                  int depth = 8,      // ↑ De 4 a 5
                  double g = 0.92,    // ↓ De 0.95 a 0.92
                  double c = 1.2,     // ↓ De 1.4 a 1.2
                  int iters = 300)    // ↑ De 100 a 200
        : myId(id), maxDepth(depth), gamma(g), Cp(c), iterationsPerUpdate(iters), root(nullptr) {}

    bool requiresContinuousPlanning() const override { 
        return true;
    }

    std::optional<Distribution> performBackgroundPlanning(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        ensureTreeValidity(obs, scenario);
        runMCTSIterations(obs, scenario, iterationsPerUpdate);
        optimizeDistribution();
        
        if (myCurrentDistribution.empty()) {
            return std::nullopt;
        }
        return myCurrentDistribution;
    }

    Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
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

private:
    // Motor principal MCTS
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
};

} // namespace tau