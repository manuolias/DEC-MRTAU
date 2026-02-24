#include "tau/solver_greedy.hpp"
#include <limits> // Para std::numeric_limits
#include <iostream>

namespace tau {

GreedySolver::GreedySolver(RobotID id) : myID(id) {}

Action GreedySolver::decideNextAction(const LocalBelief& belief, const std::shared_ptr<Scenario>& scenario) {
    // 1. Condición de parada: Si no hay tareas pendientes, ir a la estación y terminar
    if (belief.pendingTasks.empty()) {
        StationID defaultStation = 1; 
        NodeID stationNode = scenario->stations.at(defaultStation).node;
        return Action::Finish(stationNode); 
    }

    // 2. Variables para buscar la tarea más cercana
    TaskID closestTask = -1;
    double minDistance = std::numeric_limits<double>::max(); // Infinito inicial
    NodeID myNode = belief.currentNode;

    // 3. Evaluar todas las tareas que el robot percibe como libres
    for (TaskID taskID : belief.pendingTasks) {
        const TaskInfo& taskInfo = scenario->tasks.at(taskID);
        NodeID taskNode = taskInfo.node;

        double distance = 0.0;
        
        // Si el robot ya está en el nodo de la tarea, la distancia es 0
        if (myNode != taskNode) {
            // Consultamos el diccionario de distancias físicas del escenario
            distance = scenario->nodes.at(myNode).neighbors.at(taskNode);
        }

        // ¿Es esta tarea la más cercana hasta ahora?
        if (distance < minDistance) {
            minDistance = distance;
            closestTask = taskID;
        }
    }

    // 4. Devolver la acción de ir a ejecutar la tarea ganadora
    NodeID targetNode = scenario->tasks.at(closestTask).node;
    return Action::Execute(targetNode, closestTask);
}

} // namespace tau