#pragma once

#include "solver_interface.hpp"
#include "utils/estimation.hpp"
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

namespace tau {

class CBBASolver : public ISolver {
private:
    RobotID myId;
    const int MAX_BUNDLE_SIZE = 4; // Límite de tareas en el paquete

    // Función auxiliar: Calcula la puja marginal de una sola tarea
    double calculateMarginalBid(NodeID virtualNode, Time virtualTime, TaskID tId, 
                            const std::shared_ptr<const Scenario>& scenario, 
                            const std::map<TaskID, Task>& tasks, // <-- ¡NUEVO PARÁMETRO!
                            RobotID rId) {
    
    const auto& taskInfo = scenario->getTasks().at(tId);
    NodeID taskNode = taskInfo.node;
    
    Distance dist = scenario->distanceBetween(virtualNode, taskNode);
    Time travelTime = dist / scenario->robots.at(rId).navigationVelocity;
    Time estimatedArrival = virtualTime + travelTime;
    
    // Ventana de Ejecución
    if (estimatedArrival > taskInfo.latestStart) {
        return 0.0;
    }

    Time startTime = std::max(estimatedArrival, taskInfo.earliestStart);
    Time waitTime = startTime - estimatedArrival;

    // Tiempos y Probabilidad
    double p = taskInfo.successProb;
    Time expectedExecTime = (p * taskInfo.averageSuccessTime) + ((1.0 - p) * taskInfo.averageFailTime);
    
    // Inversión de Tiempo
    double totalTimeInvestment = travelTime + waitTime + expectedExecTime;
    double baseBid = (p * 10000.0) / (1.0 + totalTimeInvestment);

    // Factor de Cooperación
    int currentWorkers = 0;
    auto taskIt = tasks.find(tId);
    if (taskIt != tasks.end()) {
        currentWorkers = taskIt->second.assignedWorkers;
    }

    double myContribution = currentWorkers + 1.0;
    double coopRatio = myContribution / static_cast<double>(taskInfo.requiredWorkers);
    
    if (coopRatio > 1.0) {
        coopRatio = 1.0;
    }

    return baseBid * coopRatio;
}

    // Retorna el bundle y un mapa con la puja marginal de cada tarea en el bundle
    std::pair<std::vector<TaskID>, std::map<TaskID, double>> buildBestBundle(
        RobotID rId, const Robot& startState, const std::set<TaskID>& blacklist,
        const std::shared_ptr<const Scenario>& scenario, 
        const std::map<TaskID, Task>& tasks, Time currentTime,
        const Observation& obs) {

        std::vector<TaskID> bundle;
        std::map<TaskID, double> taskBids;
        
        if (startState.status == RobotStatus::FAILED || startState.status == RobotStatus::FINISHED) {
            return {bundle, taskBids};
        }

        // Estado virtual que irá avanzando
        NodeID vNode = startState.node;
        // Preferimos la estimación precalculada en la Observation
        const auto& nextMap = obs.getNextDecisionInfo();
        auto it = nextMap.find(rId);
        Time vTime = (it != nextMap.end()) ? it->second.first : utils::estimateNextDecisionTime(startState, scenario->getTasks(), tasks, currentTime);
        BatteryLevel vBattery = startState.batteryLevel;
        Velocity vel = scenario->robots.at(rId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(rId).batteryRateWhileNavigating;

        for (int step = 0; step < MAX_BUNDLE_SIZE; ++step) {
            TaskID bestTask = NULL_ID;
            double bestMarginal = 0.0;
            Time bestArrival = 0.0;
            BatteryLevel bestCost = 0.0;

            for (const auto& [tId, task] : tasks) {
                if (task.status != TaskStatus::PENDING) continue;
                // No repetir tareas que ya están en el bundle
                if (std::find(bundle.begin(), bundle.end(), tId) != bundle.end()) continue;
                 // La "lista negra" SOLO aplica a la primera tarea del paquete (step == 0)
                if (step == 0 && blacklist.count(tId)) continue;

                NodeID taskNode = scenario->getTasks().at(tId).node;
                Distance dist = scenario->distanceBetween(vNode, taskNode);
                BatteryLevel cost = (dist / vel) * rate;

                if (cost <= vBattery) {
                    double bid = calculateMarginalBid(vNode, vTime, tId, scenario, tasks, rId);
                    if (bid > bestMarginal) {
                        bestMarginal = bid;
                        bestTask = tId;
                        bestArrival = vTime + (dist / vel);
                        bestCost = cost;
                    }
                }
            }

            if (bestTask != NULL_ID) {
                bundle.push_back(bestTask);
                taskBids[bestTask] = bestMarginal; // Añadimos la utilidad al bid total del bundle

                // Actualizamos el estado virtual asumiendo que ejecutamos esta tarea
                const auto& tInfo = scenario->getTasks().at(bestTask);
                vNode = tInfo.node;
                Time duration = (tInfo.successProb * tInfo.averageSuccessTime) + ((1.0 - tInfo.successProb) * tInfo.averageFailTime);
                BatteryLevel demand = (tInfo.successProb * tInfo.averageSuccessDemand) + ((1.0 - tInfo.successProb) * tInfo.averageFailDemand);
                
                vTime = std::max(bestArrival, tInfo.earliestStart) + duration;
                vBattery -= (bestCost + demand);
                
                if (vBattery <= 0.0) break; // Si nos quedamos sin batería mental, paramos de añadir
            } else {
                break; // No hay más tareas alcanzables
            }
        }

        return {bundle, taskBids};
    }

public:
    CBBASolver(RobotID id) : myId(id) {}

    Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        const auto& myState = obs.getMyState();
        BatteryLevel currentBattery = myState.batteryLevel;
        Velocity vel = scenario->robots.at(myId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(myId).batteryRateWhileNavigating;
        
        bool pendingTasksExist = false;
        bool unreachableTasksExist = false;

        // ESCANEO PREVIO DEL MUNDO. 
        // De esta manera voy saber, en el caso de no asignar ninguna tarea al robot, en que caso me encuentro:
        // CASO 1: Si llego a todas las tareas pendientes, pero mis vecinos me han ganado
        // CASO 2: Si el mundo tiene tareas a las que no llego, mi mejor opción es recargar y probar de nuevo.
        // CASO 3: Si no quedan tareas pendientes, simplemente termino.

        for (const auto& [tId, task] : obs.getKnownTasks()) {
            if (task.status == TaskStatus::PENDING) {
                pendingTasksExist = true;

                NodeID taskNode = scenario->getTasks().at(tId).node;
                Distance dist = scenario->distanceBetween(myState.node, taskNode);
                BatteryLevel cost = (dist / vel) * rate;

                // Si hay al menos una tarea a la que no llego por batería, levanto la bandera
                if (cost > currentBattery) {
                    unreachableTasksExist = true;
                }
            }
        }

        // CASO 3: Simplemente no quedan tareas
        if (!pendingTasksExist) {
            return Action(Action::Type::FINISH);
        }

        std::set<TaskID> rejectedFirstTasks;

        // BUCLE PRINCIPAL: Iteramos hasta que nos asignemos o nos quedemos sin opciones
        while (true) {

            // 1. Calculamos nuestro Bundle, excluyendo las tareas que ya nos han rechazado
            auto [myBundle, myBids] = buildBestBundle(myId, myState, rejectedFirstTasks, scenario, obs.getKnownTasks(), obs.getCurrentTime(), obs);

            // Si el bundle está vacío, (CASO 1 o CASO 2).
            if (myBundle.empty()) {
                // CASO 2: Si el mundo tiene tareas a las que no llego, mi mejor opción es recargar y probar suerte después.
                if (unreachableTasksExist) {
                    return Action(Action::Type::RECHARGE);
                }
                
                // CASO 1: Si llego a todas las tareas pendientes, pero mis vecinos me han ganado
                // limpiamente en todas, significa que soy irrelevante. Termino.
                return Action(Action::Type::FINISH);
            }

            TaskID targetTask = myBundle.front();
            double myBidForTarget = myBids[targetTask];

            // 2. Comunicamos y escuchamos a los vecinos 
            int betterBidsCount = 0;

            for (const auto& [otherId, otherRobot] : obs.getKnownRobots()) {

                // Si el vecino ya está asignado o ejecutando ESA tarea, no compite por las plazas restantes
                if (otherId == myId || otherRobot.onTask == targetTask) continue;
                if (otherRobot.status == RobotStatus::FAILED || otherRobot.status == RobotStatus::FINISHED) continue;

                // El vecino nos responde con su mejor bundle
                auto [theirBundle, theirBids] = buildBestBundle(otherId, otherRobot, {}, scenario, obs.getKnownTasks(), obs.getCurrentTime(), obs);
                
                // CONDICIÓN DEL USUARIO: ¿Nos supera y compite por la MISMA primera tarea?
                if (theirBids.find(targetTask) != theirBids.end()) {
                    double theirBidForTarget = theirBids[targetTask];
                    
                    if (theirBidForTarget > myBidForTarget) {
                        betterBidsCount++;
                    }
                }
            }

            // 3. Verificamos disponibilidad de plazas
            int required = scenario->getTasks().at(targetTask).requiredWorkers;
            int assigned = obs.getKnownTasks().at(targetTask).assignedWorkers;
            int availableSlots = required - assigned;

            if (availableSlots > 0 && betterBidsCount < availableSlots) {
                // ¡Éxito! Hemos superado el consenso para la primera tarea de nuestro Bundle
                return Action(Action::Type::EXECUTE_TASK, targetTask);
            } else {
                // 4. Fracaso. Alguien con un Bundle mejor nos ha quitado la plaza.
                // Metemos la tarea en la lista negra y el bucle while vuelve a empezar.
                rejectedFirstTasks.insert(targetTask);
            }
        }
    }
};

} // namespace tau