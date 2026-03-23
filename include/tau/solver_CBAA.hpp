#pragma once

#include "solver_interface.hpp"
#include "utils/estimation.hpp"
#include <map>
#include <vector>
#include <algorithm>

namespace tau {

class CBAASolver : public ISolver {
private:
    RobotID myId;
    std::vector<Message> outbox;

    // Lógica interna del robot para calcular su propia puja
    double calculateBid(const Robot& robot, RobotID rId, TaskID tId, 
                        const std::shared_ptr<const Scenario>& scenario, 
                        const std::map<TaskID, Task>& tasks, Time currentTime) {
        
        NodeID robotNode = robot.node;
        NodeID taskNode = scenario->getTasks().at(tId).node;
        
        Distance dist = (robotNode == taskNode) ? 0.0 : scenario->getNodes().at(robotNode).neighbors.at(taskNode);
        Time travelTime = dist / scenario->robots.at(rId).navigationVelocity;
        
        // Estimamos cuándo estará libre (si está ocupado)
        Time freeTime = utils::estimateNextDecisionTime(robot, scenario->getTasks(), tasks, currentTime);
        Time estimatedArrival = freeTime + travelTime;
        
        // Si no llega a tiempo antes de que caduque, puja 0
        if (estimatedArrival > scenario->getTasks().at(tId).latestStart) {
            return 0.0;
        }

        // Fomentamos tareas rápidas y cercanas
        return 10000.0 / (1.0 + estimatedArrival);
    }

    // Emula la recepción de la petición por parte del vecino y su respuesta instantánea (Abstracción de red)
    Message generateNeighborReply(RobotID neighborId, const Robot& neighborState, TaskID requestedTask, 
                                  const std::shared_ptr<const Scenario>& scenario, 
                                  const std::map<TaskID, Task>& tasks, Time currentTime) {
        
        Message reply;
        reply.sender = neighborId;
        reply.receiver = myId;
        reply.type = BID_REPLY;
        reply.targetTask = requestedTask;
        
        // Si el vecino está roto, responde con puja 0
        if (neighborState.status == RobotStatus::FAILED) {
            reply.value = 0.0;
            return reply;
        }

        // El vecino evalúa cuánto tardaría él
        reply.value = calculateBid(neighborState, neighborId, requestedTask, scenario, tasks, currentTime);
        return reply;
    }

public:
    CBAASolver(RobotID id) : myId(id) {}

    Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        outbox.clear();
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
                Distance dist = (myState.node == taskNode) ? 0.0 : scenario->getNodes().at(myState.node).neighbors.at(taskNode);
                BatteryLevel cost = (dist / vel) * rate;

                // Si la batería es suficiente, calculamos puja
                if (cost <= currentBattery) {
                    double bid = calculateBid(myState, myId, tId, scenario, obs.getKnownTasks(), obs.getCurrentTime());
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

        // 3. Negociación con los vecinos
        for (const auto& [tId, myBid] : myBids) {
            
            // a) Emitimos mensaje conceptual (Request)
            Message requestMsg;
            requestMsg.sender = myId;
            requestMsg.receiver = NULL_ID; // Broadcast
            requestMsg.type = BID_REQUEST;
            requestMsg.targetTask = tId;
            outbox.push_back(requestMsg); // Lo dejamos registrado en el log de comunicaciones

            // b) Recopilamos las respuestas (Replies)
            std::vector<Message> replies;
            for (const auto& [otherId, otherRobot] : obs.getKnownRobots()) {
                if (otherId == myId) continue;
                if (otherRobot.status == RobotStatus::FAILED || otherRobot.status == RobotStatus::FINISHED) continue; // Rotos no compiten
                
                // ¡LA CORRECCIÓN CLAVE! 
                // Si este vecino YA está en la tarea, ya tiene su plaza. 
                // No compite por las plazas restantes, así que no le pedimos puja.
                if (otherRobot.onTask == tId) continue;
                // Los vecinos evalúan y comunican su puja de vuelta
                Message reply = generateNeighborReply(otherId, otherRobot, tId, scenario, obs.getKnownTasks(), obs.getCurrentTime());
                replies.push_back(reply);
            }

            // c) Ordenamos y determinamos si hay hueco (Consenso)
            int betterBidsCount = 0;
            for (const auto& reply : replies) {
                // Si la puja del vecino es mayor
                if (reply.value > myBid) {
                    betterBidsCount++;
                }
            }

            // Calculamos los huecos reales que quedan en la tarea
            int required = scenario->getTasks().at(tId).requiredWorkers;
            int assigned = obs.getKnownTasks().at(tId).assignedWorkers;
            int availableSlots = required - assigned;
            
            // ¿Entramos en el Top N?
            if (betterBidsCount < availableSlots) {
                return Action(Action::Type::EXECUTE_TASK, tId);
            }
            
            // Si nos rechazan (betterBidsCount >= required), pasamos a la siguiente tarea del bucle.
        }

        // Si se agota el diccionario sin éxito, recargamos o terminamos
        if (unreachableTasksExist) {
            return Action(Action::Type::RECHARGE);
        }
        
        return Action(Action::Type::FINISH);
    }

    void receiveMessage(const Message& msg) override {}
    std::vector<Message> getOutbox() override { return {}; }
};

} // namespace tau