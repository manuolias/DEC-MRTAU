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

    // --- Hiperparámetros del algoritmo ---
    int maxDepth;               // Profundidad máxima del árbol (Horizonte)
    double gamma;               // Factor de descuento temporal para D-UCT (ej. 0.9)
    double Cp;                  // Constante de exploración del D-UCT (ej. 1.0)
    int iterationsPerUpdate;    // Cuántas ramas exploramos por cada latido de background

    // El árbol de búsqueda se mantiene entre llamadas para reaprovechar conocimiento
    std::shared_ptr<MCTSNode> root;
    
    // Distribución actual de mis propios planes (Pizarra local)
    Distribution myCurrentDistribution;

    // =========================================================
    // BLOQUE 1: LÓGICA INTERNA DEL MCTS
    // =========================================================

    // Genera las acciones válidas desde un estado virtual (Filtrando por batería, etc.)
    std::vector<Action> getPossibleActions(const Robot& virtualState, Time virtualTime, 
                                           const std::shared_ptr<const Scenario>& scenario, 
                                           const Observation& obs,
                                           const std::set<TaskID>& tasksInBranch) { // <-- NUEVO PARÁMETRO VITAL
        
        std::vector<Action> possibleActions;
        
        Velocity vel = scenario->robots.at(myId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(myId).batteryRateWhileNavigating;
        BatteryLevel currentVirtualBattery = virtualState.batteryLevel;
        NodeID currentNode = virtualState.node;

        // 1. Añadir tareas PENDING alcanzables
        for (const auto& [tId, task] : obs.getKnownTasks()) {
            // A) Filtro de realidad: Solo tareas pendientes
            if (task.status != TaskStatus::PENDING) continue;
            
            // B) Filtro mental: Ignorar si ya la hemos "hecho" en esta rama del árbol
            if (tasksInBranch.count(tId)) continue;

            NodeID taskNode = scenario->getTasks().at(tId).node;
            Distance dist = (currentNode == taskNode) ? 0.0 : scenario->getNodes().at(currentNode).neighbors.at(taskNode);
            Time travelTime = dist / vel;
            BatteryLevel cost = travelTime * rate;
            Time estimatedArrival = virtualTime + travelTime;

            // C) Filtro físico: ¿Llegamos vivos y antes de que la tarea caduque?
            if (cost <= currentVirtualBattery && estimatedArrival <= scenario->getTasks().at(tId).latestStart) {
                possibleActions.push_back(Action(Action::Type::EXECUTE_TASK, tId));
            }
        }

        // 2. Añadir RECHARGE si la batería no está a tope (ej. menor al 95%)
        double capacity = scenario->robots.at(myId).batteryCapacity;
        if (currentVirtualBattery < capacity * 0.95) {
            StationID nearestStationID = scenario->getNodes().at(currentNode).nearestStation;
            NodeID stationNode = scenario->getStations().at(nearestStationID).node;
            Distance distToStation = (currentNode == stationNode) ? 0.0 : scenario->getNodes().at(currentNode).neighbors.at(stationNode);
            BatteryLevel costToStation = (distToStation / vel) * rate;

            // ¿Llegamos vivos a la estación de recarga?
            if (costToStation <= currentVirtualBattery) {
                possibleActions.push_back(Action(Action::Type::RECHARGE));
            }
        }

        // 3. Añadir FINISH si no hay más opciones
        // Esto fuerza al MCTS a no crear ramas infinitas muertas. 
        // Si no podemos hacer nada más, la única acción válida es terminar.
        if (possibleActions.empty()) {
            possibleActions.push_back(Action(Action::Type::FINISH));
        }

        return possibleActions;
    }

    // Fase 1: Selección (D-UCT)
    std::shared_ptr<MCTSNode> selectNode(std::shared_ptr<MCTSNode> currentNode) {
        
        // Bajamos por el árbol mientras el nodo esté completamente expandido y no sea terminal.
        // Si no está completamente expandido, significa que tiene untriedActions y debemos parar
        // para que la Fase 2 (expandNode) cree un hijo nuevo.
        while (currentNode->isFullyExpanded() && !currentNode->isTerminal()) {
            
            std::shared_ptr<MCTSNode> bestChild = nullptr;
            double bestScore = -std::numeric_limits<double>::infinity();

            // t_{parent}(\gamma): Visitas con descuento del nodo padre
            double parentVisits = currentNode->getDiscountedVisits();
            
            // Protección matemática contra log(0) o valores negativos si el descuento baja mucho el contador
            double logParent = (parentVisits > 1.0) ? std::log(parentVisits) : 0.0;

            // Evaluamos la fórmula D-UCT para cada hijo
            for (const auto& child : currentNode->getChildren()) {
                
                // t_j(\gamma): Visitas con descuento del nodo hijo
                double childVisits = child->getDiscountedVisits();

                double uctScore;
                
                // Si un hijo apenas tiene visitas (o por el decaimiento de gamma se ha quedado a 0),
                // le damos prioridad absoluta (infinito) para forzar su re-exploración.
                if (childVisits <= 0.0) {
                    uctScore = std::numeric_limits<double>::infinity();
                } else {
                    // \bar{F}_j(\gamma): Recompensa media descontada (Término de Explotación)
                    double exploitation = child->getExpectedReward();
                    
                    // Término de Exploración descontado
                    double exploration = 2.0 * Cp * std::sqrt(logParent / childVisits);
                    
                    uctScore = exploitation + exploration;
                }

                // Nos quedamos con el hijo que maximice la ecuación D-UCB
                if (uctScore > bestScore) {
                    bestScore = uctScore;
                    bestChild = child;
                }
            }

            // Si por algún motivo extraño no encontramos hijo, rompemos el bucle por seguridad
            if (!bestChild) break;

            // Avanzamos al mejor hijo y repetimos el proceso
            currentNode = bestChild;
        }

        return currentNode;
    }

    // Fase 2: Expansión
    std::shared_ptr<MCTSNode> expandNode(std::shared_ptr<MCTSNode> node, 
                                         const std::shared_ptr<const Scenario>& scenario,
                                         const Observation& obs) { // <-- Añadido obs para poder llamar a getPossibleActions
        
        // 1. Extraemos una acción inexplorada del nodo actual
        Action action = node->popUntriedAction();

        // 2. Copiamos el estado virtual del padre para empezar a simular
        Robot virtualState = node->getVirtualState();
        Time virtualTime = node->getVirtualTime();

        Velocity vel = scenario->robots.at(myId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(myId).batteryRateWhileNavigating;
        NodeID currentNode = virtualState.node;

        // 3. Simulación Determinista (Tu idea brillante)
        if (action.getType() == Action::Type::EXECUTE_TASK) {
            TaskID tId = action.getTarget();
            const auto& taskInfo = scenario->getTasks().at(tId);
            
            // Navegación
            Distance dist = (currentNode == taskInfo.node) ? 0.0 : scenario->getNodes().at(currentNode).neighbors.at(taskInfo.node);
            Time travelTime = dist / vel;
            BatteryLevel travelCost = travelTime * rate;

            // Ejecución Esperada (Determinista)
            double p = taskInfo.successProb;
            Time expectedExecTime = (p * taskInfo.averageSuccessTime) + ((1.0 - p) * taskInfo.averageFailTime);
            BatteryLevel expectedExecCost = (p * taskInfo.averageSuccessDemand) + ((1.0 - p) * taskInfo.averageFailDemand);

            Time arrivalTime = virtualTime + travelTime;
            Time startTime = std::max(arrivalTime, taskInfo.earliestStart); // Esperamos si llegamos pronto

            // Actualizamos el estado virtual
            virtualTime = startTime + expectedExecTime;
            virtualState.batteryLevel -= (travelCost + expectedExecCost);
            virtualState.node = taskInfo.node;

        } else if (action.getType() == Action::Type::RECHARGE || action.getType() == Action::Type::FINISH) {
            // Buscamos la estación más cercana
            StationID nearestStationID = scenario->getNodes().at(currentNode).nearestStation;
            NodeID stationNode = scenario->getStations().at(nearestStationID).node;
            
            Distance dist = (currentNode == stationNode) ? 0.0 : scenario->getNodes().at(currentNode).neighbors.at(stationNode);
            Time travelTime = dist / vel;
            BatteryLevel travelCost = travelTime * rate;

            virtualTime += travelTime;
            virtualState.node = stationNode;
            
            if (action.getType() == Action::Type::RECHARGE) {
                virtualState.batteryLevel = scenario->robots.at(myId).batteryCapacity;
            } else {
                virtualState.batteryLevel -= travelCost; // El FINISH asume que viaja a la estación y se apaga
            }
        }

        // 4. Recopilamos las tareas que ya hemos hecho en esta rama (para evitar bucles mentales)
        std::set<TaskID> tasksInBranch;
        std::shared_ptr<MCTSNode> curr = node;
        while (curr != nullptr) {
            if (curr->getAction().getType() == Action::Type::EXECUTE_TASK) {
                tasksInBranch.insert(curr->getAction().getTarget());
            }
            curr = curr->getParent();
        }
        // Metemos la tarea actual si es EXECUTE_TASK
        if (action.getType() == Action::Type::EXECUTE_TASK) {
            tasksInBranch.insert(action.getTarget());
        }

        // 5. Calculamos qué se puede hacer desde este nuevo futuro
        std::vector<Action> newPossibleActions;
        
        // Si la acción fue FINISH, es un nodo estrictamente terminal (no hay posibles acciones)
        if (action.getType() != Action::Type::FINISH) {
            // Comprobación del horizonte (maxDepth). Si llegamos al límite, cortamos el árbol.
            // Calculamos la profundidad que tendrá el hijo (node->getDepth() + 1)
            if (maxDepth == 0 || (node->getDepth() + 1) < maxDepth) {
                newPossibleActions = getPossibleActions(virtualState, virtualTime, scenario, obs, tasksInBranch);
            }
        }

        // 6. Instanciamos el nuevo hijo, lo vinculamos y lo devolvemos
        auto child = std::make_shared<MCTSNode>(node, action, virtualState, virtualTime, newPossibleActions);
        node->addChild(child);

        return child;
    }

    // Fase 3: Simulación / Evaluación Determinista (Certainty Equivalence)
    double evaluateLeaf(std::shared_ptr<MCTSNode> leafNode, 
                        const Observation& obs, 
                        const std::shared_ptr<const Scenario>& scenario) {
        
        double totalReward = 0.0;
        
        // 1. Recopilar la secuencia de tareas que hemos decidido hacer en esta rama
        std::vector<TaskID> branchTasks;
        std::shared_ptr<MCTSNode> curr = leafNode;
        while (curr != nullptr) {
            if (curr->getAction().getType() == Action::Type::EXECUTE_TASK) {
                branchTasks.push_back(curr->getAction().getTarget());
            }
            curr = curr->getParent();
        }

        // 2. Evaluar el valor aportado por cada tarea
        for (TaskID tId : branchTasks) {
            const auto& taskInfo = scenario->getTasks().at(tId);
            
            // Valor Base: Podemos asumir 1000.0 como base, ponderado por su probabilidad de éxito
            double baseValue = taskInfo.successProb * 1000.0; 

            // --- LA MAGIA DEC-MCTS: Predecir la coordinación con la Pizarra Pública ---
            // Empezamos con 1 trabajador (nosotros) + los que ya estén asignados físicamente
            double expectedWorkers = 1.0 + obs.getKnownTasks().at(tId).assignedWorkers;

            // Consultamos las distribuciones (creencias) comunicadas por los vecinos
            for (const auto& [otherId, distribution] : obs.getKnownDistributions()) {
                if (otherId == myId) continue;
                
                double probOfComing = 0.0;
                // NOTA: Asumo que distribution se puede iterar devolviendo pares <SecuenciaTareas, Probabilidad>
                // Si tu clase Distribution tiene otra interfaz, ajústalo aquí.
                for (const auto& [bundle, prob] : distribution.getProbabilities()) {
                    // Si el vecino tiene esta tarea en su paquete, sumamos la probabilidad
                    if (std::find(bundle.begin(), bundle.end(), tId) != bundle.end()) {
                        probOfComing += prob;
                    }
                }
                expectedWorkers += probOfComing;
            }

            // Factor de Coordinación: Si no llegamos al mínimo requerido, penalizamos el valor
            if (expectedWorkers < taskInfo.requiredWorkers) {
                // Penalización severa (cuadrática o lineal) por falta de personal
                double ratio = expectedWorkers / static_cast<double>(taskInfo.requiredWorkers);
                baseValue *= (ratio * ratio); // Ej: Si esperamos 1 y piden 2, el valor cae al 25%
            } else {
                // Bonus ligero si hay exceso de personal (fomenta la robustez)
                baseValue *= 1.1; 
            }

            totalReward += baseValue;
        }

        // 3. Penalización por coste (Eficiencia)
        // Restamos el tiempo que tardaríamos en ejecutar esta rama. 
        // A igualdad de tareas resueltas, preferimos la rama que tarde menos tiempo (virtualTime menor).
        Time timeSpent = leafNode->getVirtualTime() - obs.getCurrentTime();
        if (timeSpent > 0.0) {
            totalReward -= (timeSpent * 0.5); // Factor de penalización por tiempo
        }

        // 4. Aseguramos que la recompensa nunca sea negativa para no romper la fórmula UCB
        return std::max(0.0, totalReward);
    }

    // Fase 4: Retropropagación
    void backpropagate(std::shared_ptr<MCTSNode> node, double reward) {
        std::shared_ptr<MCTSNode> current = node;
        
        // Subimos por el árbol hasta que lleguemos al padre de la raíz (que es nullptr)
        while (current != nullptr) {
            // Aplicamos la matemática del D-UCT (decaimiento + nueva recompensa)
            current->update(reward, gamma);
            
            // Subimos al siguiente nivel
            current = current->getParent();
        }
    }

    // =========================================================
    // MOTOR PRINCIPAL DEL MCTS
    // =========================================================
    
    // Función que agrupa las 4 fases y hace crecer el árbol
    void runMCTSIterations(const Observation& obs, const std::shared_ptr<const Scenario>& scenario, int numIterations) {
        
        for (int i = 0; i < numIterations; ++i) {
            // Fase 1: SELECCIÓN
            // Bajamos por las mejores ramas guiados por la fórmula D-UCT
            auto node = selectNode(root);
            
            // Fase 2: EXPANSIÓN
            // Si el nodo seleccionado no es un final de trayecto (FINISH o límite de profundidad),
            // sacamos una acción inexplorada y creamos un nuevo futuro (hijo).
            if (!node->isTerminal()) {
                node = expandNode(node, scenario, obs);
            }
            
            // Fase 3: EVALUACIÓN (Certainty Equivalence)
            // Calculamos el valor determinista de este futuro, consultando a la pizarra pública.
            double reward = evaluateLeaf(node, obs, scenario);
            
            // Fase 4: RETROPROPAGACIÓN
            // Subimos la recompensa hasta la raíz para que las estadísticas se actualicen.
            backpropagate(node, reward);
        }
    }

    // =========================================================
    // BLOQUE 2: OPTIMIZACIÓN VARIACIONAL (Dec-MCTS)
    // =========================================================

    // Función auxiliar para extraer el "Bundle" (secuencia de tareas) de un nodo
    Bundle extractBundleFromNode(std::shared_ptr<MCTSNode> node) {
        Bundle bundle;
        std::shared_ptr<MCTSNode> curr = node;
        while (curr != nullptr) {
            if (curr->getAction().getType() == Action::Type::EXECUTE_TASK) {
                // Lo insertamos al principio porque vamos subiendo desde la hoja a la raíz
                bundle.insert(bundle.begin(), curr->getAction().getTarget());
            }
            curr = curr->getParent();
        }
        return bundle;
    }

    // Función auxiliar para recorrer el árbol y extraer todos los nodos
    void collectAllNodes(std::shared_ptr<MCTSNode> node, std::vector<std::shared_ptr<MCTSNode>>& allNodes) {
        if (!node) return;
        
        // Solo guardamos nodos que representen un camino con al menos una tarea
        if (node->getDepth() > 0 && node->getDiscountedVisits() > 0) {
            allNodes.push_back(node);
        }
        
        for (const auto& child : node->getChildren()) {
            collectAllNodes(child, allNodes);
        }
    }

    void optimizeDistribution() {
        if (!root) return;

        // 1. Recopilamos todos los nodos del árbol
        std::vector<std::shared_ptr<MCTSNode>> allNodes;
        collectAllNodes(root, allNodes);

        if (allNodes.empty()) return;

        // 2. Ordenamos los nodos por su Recompensa Esperada (D-UCT) de mayor a menor
        std::sort(allNodes.begin(), allNodes.end(), 
            [](const std::shared_ptr<MCTSNode>& a, const std::shared_ptr<MCTSNode>& b) {
                return a->getExpectedReward() > b->getExpectedReward();
            });

        // 3. Compresión del árbol: Nos quedamos con el Top 10 (como sugiere el paper original)
        const size_t MAX_DISTRIBUTION_SIZE = 10;
        size_t limit = std::min(allNodes.size(), MAX_DISTRIBUTION_SIZE);

        myCurrentDistribution.clear();
        
        // Vamos a usar Softmax para calcular las probabilidades basándonos en la recompensa.
        // Usamos una "temperatura" (beta) similar a la que proponen en el artículo para suavizar la entropía.
        double beta = 100.0; // Ajustable: mayor valor = probabilidades más planas (más entropía)
        
        double maxReward = allNodes[0]->getExpectedReward(); // Para estabilidad numérica del Softmax
        double sumExp = 0.0;
        
        std::vector<std::pair<Bundle, double>> expValues;

        // 4. Extraemos los bundles y calculamos el exponente
        for (size_t i = 0; i < limit; ++i) {
            Bundle bundle = extractBundleFromNode(allNodes[i]);
            
            // Si el bundle está vacío (ej. era solo RECHARGE), lo ignoramos para la distribución de tareas
            if (bundle.empty()) continue;

            double reward = allNodes[i]->getExpectedReward();
            double expVal = std::exp((reward - maxReward) / beta);
            
            expValues.push_back({bundle, expVal});
            sumExp += expVal;
        }

        // 5. Normalizamos para que la suma de probabilidades sea exactamente 1.0
        for (const auto& [bundle, expVal] : expValues) {
            double probability = expVal / sumExp;
            
            // Si el mismo bundle aparece por dos ramas diferentes (ej. ir a recargar en un caso y en otro no)
            // sumamos sus probabilidades.
            myCurrentDistribution[bundle] += probability; 
        }
    }

    // =========================================================
    // BLOQUE 3: INTERFAZ CON EL SIMULADOR
    // =========================================================

        // Función auxiliar para inicializar o resetear el árbol si el estado físico no coincide
    void ensureTreeValidity(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) {
        // Comprobamos si no hay raíz, o si el reloj virtual de la raíz se ha desincronizado
        // del reloj físico (por ejemplo, porque acabamos de terminar una tarea en la realidad).
        if (!root || std::abs(root->getVirtualTime() - obs.getCurrentTime()) > 1e-5) {
            
            // Creamos una nueva semilla desde el presente absoluto
            std::set<TaskID> emptyBranch;
            std::vector<Action> initialActions = getPossibleActions(obs.getMyState(), obs.getCurrentTime(), scenario, obs, emptyBranch);
            
            root = std::make_shared<MCTSNode>(obs.getMyState(), obs.getCurrentTime(), initialActions);
        }
    }

public:
    DecMCTSSolver(RobotID id, int depth = 0, double g = 0.9, double c = 1.0, int iters = 500) 
        : myId(id), maxDepth(depth), gamma(g), Cp(c), iterationsPerUpdate(iters), root(nullptr) {}

    // ¡Vital para que el simulador nos lance eventos de background!
    bool requiresContinuousPlanning() const override { 
        return true; 
    }

    // Latido en segundo plano (Anytime Planning)
    std::optional<Distribution> performBackgroundPlanning(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        
        // 1. Aseguramos que el árbol parta de nuestra realidad actual
        ensureTreeValidity(obs, scenario);

        // 2. Pensamos: Hacemos crecer el árbol con N iteraciones
        runMCTSIterations(obs, scenario, iterationsPerUpdate);

        // 3. Reflexionamos: Extraemos las mejores ramas y las convertimos en probabilidades
        optimizeDistribution();

        // 4. Comunicamos: Devolvemos nuestra creencia para que el simulador la publique
        if (myCurrentDistribution.empty()) {
            return std::nullopt;
        }
        return myCurrentDistribution;
    }

    // Decisión física (Momento de la verdad)
    Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        
        // 1. Aseguramos que el árbol sea válido
        ensureTreeValidity(obs, scenario);

        // 2. Último sprint de iteraciones con los datos más recientes de la pizarra global
        runMCTSIterations(obs, scenario, iterationsPerUpdate);

        // 3. Elegimos el mejor hijo directo de la raíz.
        // En MCTS, el criterio más robusto para elegir la acción real no es la recompensa,
        // sino el NÚMERO DE VISITAS. Si una rama ha sido visitada muchas veces, significa
        // que ha sobrevivido consistentemente a la ecuación D-UCT frente a otras alternativas.
        std::shared_ptr<MCTSNode> bestChild = nullptr;
        double maxVisits = -1.0;

        for (const auto& child : root->getChildren()) {
            if (child->getDiscountedVisits() > maxVisits) {
                maxVisits = child->getDiscountedVisits();
                bestChild = child;
            }
        }

        // Caso de seguridad: Si no hay hijos (ej. no se pudo expandir nada), terminamos.
        if (!bestChild) {
            return Action(Action::Type::FINISH);
        }

        // 4. Extraemos la acción ganadora
        Action chosenAction = bestChild->getAction();

        // 5. ¡Horizonte Deslizante! Avanzamos la raíz hacia el futuro que hemos elegido.
        // Esto permite que en el próximo performBackgroundPlanning (mientras viajamos),
        // el árbol ya tenga construida toda la rama que cuelga de esta decisión.
        root = bestChild;

        return chosenAction;
    }
};

} // namespace tau