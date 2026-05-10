#pragma once

#include "solver_interface.hpp"
#include "utils/estimation.hpp"
#include <map>
#include <vector>
#include <algorithm>
#include <cmath> // Para std::abs

namespace tau {

class CBAASolver : public ISolver {
private:
    RobotID myId;

    // Lógica interna del robot para calcular su propia puja (y emular la de los demás)
    double calculateBid(const Robot& robot, RobotID rId, TaskID tId, 
                    const std::shared_ptr<const Scenario>& scenario, 
                    const std::map<TaskID, Task>& tasks, Time currentTime,
                    const Observation& obs) {
        
    // 1. Descarte inicial de robots inactivos
    if (robot.status == RobotStatus::FAILED || robot.status == RobotStatus::FINISHED) {
        return 0.0;
    }

    const auto& taskInfo = scenario->getTasks().at(tId);
    NodeID robotNode = robot.node;
    NodeID taskNode = taskInfo.node;
    
    // Cálculo de ruta y tiempos
    Distance dist = scenario->distanceBetween(robotNode, taskNode);
    Time travelTime = dist / scenario->robots.at(rId).navigationVelocity;
    
    // Intentamos leer la estimación ya calculada en la Observation
    Time freeTime;
    const auto& nextMap = obs.getNextDecisionInfo();
    auto it = nextMap.find(rId);
    if (it != nextMap.end()) {
        freeTime = it->second.first;
    } else {
        freeTime = utils::estimateNextDecisionTime(robot, scenario->getTasks(), tasks, currentTime);
    }
    Time estimatedArrival = freeTime + travelTime;
    
    // 2. Ventana de Ejecución: Límite superior
    if (estimatedArrival > taskInfo.latestStart) {
        return 0.0; // Imposible llegar a tiempo
    }

    // 3. Ventana de Ejecución: Límite inferior (Penalización por espera)
    Time startTime = std::max(estimatedArrival, taskInfo.earliestStart);
    Time waitTime = startTime - estimatedArrival; // Tiempo que el robot estará parado esperando

    // 4. Tiempos y Probabilidades Esperadas
    double p = taskInfo.successProb;
    Time expectedExecTime = (p * taskInfo.averageSuccessTime) + ((1.0 - p) * taskInfo.averageFailTime);
    
    // 5. Cálculo del Valor Base
    // Multiplicamos por la probabilidad (tareas seguras valen más).
    // Dividimos por el tiempo TOTAL invertido (viaje + espera + ejecución).
    double totalTimeInvestment = travelTime + waitTime + expectedExecTime;
    double baseBid = (p * 10000.0) / (1.0 + totalTimeInvestment);

    // 6. Factor de Cooperación (Tu Efecto Bola de Nieve)
    int currentWorkers = 0;
    auto taskIt = tasks.find(tId);
    if (taskIt != tasks.end()) {
        currentWorkers = taskIt->second.assignedWorkers;
    }

    double myContribution = currentWorkers + 1.0;
    double coopRatio = myContribution / static_cast<double>(taskInfo.requiredWorkers);
    
    // Si la tarea requiere 3 y ya tiene 3, el ratio es > 1.0. 
    // Lo capamos a 1.0 para que un exceso de trabajadores no infle la puja artificialmente.
    if (coopRatio > 1.0) {
        coopRatio = 1.0; 
    }

    return baseBid * coopRatio;
}

public:
    CBAASolver(RobotID id) : myId(id) {}

    Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        bool pendingTasksExist = false;
        bool unreachableTasksExist = false;
        
        const auto& myState = obs.getMyState();
        BatteryLevel currentBattery = myState.batteryLevel;
        Velocity vel = scenario->robots.at(myId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(myId).batteryRateWhileNavigating;

        // 1 y 2. Crear diccionario propio y evaluar todas las tareas alcanzables
        std::vector<std::pair<TaskID, double>> myBids;

        for (const auto& [tId, task] : obs.getKnownTasks()) {
            if (task.status == TaskStatus::PENDING) {
                pendingTasksExist = true;

                NodeID taskNode = scenario->getTasks().at(tId).node;
                Distance dist = scenario->distanceBetween(myState.node, taskNode);
                BatteryLevel cost = (dist / vel) * rate;

                // Si la batería es suficiente, calculamos puja
                if (cost <= currentBattery) {
                    double bid = calculateBid(myState, myId, tId, scenario, obs.getKnownTasks(), obs.getCurrentTime(), obs);
                    if (bid > 0.0) {
                        myBids.push_back({tId, bid});
                    }
                } else {
                    unreachableTasksExist = true;
                }
            }
        }

        if (!pendingTasksExist) return Action(Action::Type::FINISH);
        if (myBids.empty()) return Action(Action::Type::RECHARGE);

        // Ordenamos las pujas de mayor a menor (Preferencias)
        std::sort(myBids.begin(), myBids.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        // 3. Negociación "Virtual" con los vecinos (Consultando el estado público)
        for (const auto& [tId, myBid] : myBids) {
            
            int betterBidsCount = 0;
            
            // Recopilamos las pujas de los competidores
            for (const auto& [otherId, otherRobot] : obs.getKnownRobots()) {
                if (otherId == myId) continue;
                if (otherRobot.status == RobotStatus::FAILED || otherRobot.status == RobotStatus::FINISHED) continue;
                
                // Si este vecino YA está en la tarea, ya tiene su plaza, no compite
                if (otherRobot.onTask == tId) continue;

                // Emulamos la puja del vecino al instante usando la información de la Observación
                double theirBid = calculateBid(otherRobot, otherId, tId, scenario, obs.getKnownTasks(), obs.getCurrentTime(), obs);
                
                // Si la puja del vecino es mayor (o en caso de empate, su ID es menor)
                if (theirBid > myBid || (std::abs(theirBid - myBid) < 1e-9 && otherId < myId)) {
                    betterBidsCount++;
                }
            }

            // Calculamos los huecos reales que quedan en la tarea
            int required = scenario->getTasks().at(tId).requiredWorkers;
            int assigned = obs.getKnownTasks().at(tId).assignedWorkers;
            int availableSlots = required - assigned;
            
            // ¿Entramos en el Top N?
            if (availableSlots > 0 && betterBidsCount < availableSlots) {
                return Action(Action::Type::EXECUTE_TASK, tId);
            }
            
            // Si nos rechazan, pasamos a la siguiente tarea del bucle.
        }

        // Si se agota el diccionario sin éxito, recargamos o terminamos
        if (unreachableTasksExist) {
            return Action(Action::Type::RECHARGE);
        }
        
        return Action(Action::Type::FINISH);
    }
};

} // namespace tau