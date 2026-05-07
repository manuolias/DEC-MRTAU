#ifndef TAU_GREEDY_SOLVER_HPP
#define TAU_GREEDY_SOLVER_HPP

#include "solver_interface.hpp"
#include <limits>
#include <vector>

namespace tau {

class GreedySolver : public ISolver {
public:
    Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) override {
        std::vector<TaskID> reachableTasks;
        bool pendingTasksExist = false;

        RobotID myId = obs.getId();
        NodeID myNode = obs.getMyState().node;
        BatteryLevel currentBattery = obs.getMyState().batteryLevel;
        
        Velocity vel = scenario->robots.at(myId).navigationVelocity;
        BatteryRate rate = scenario->robots.at(myId).batteryRateWhileNavigating;

        // 1. Buscamos tareas pendientes y comprobamos si llegamos vivos
        for (const auto& [tId, task] : obs.getKnownTasks()) {
            if (task.status == TaskStatus::PENDING) {
                pendingTasksExist = true;

                NodeID taskNode = scenario->getTasks().at(tId).node;
                Distance dist = scenario->distanceBetween(myNode, taskNode);
                Time travelTime = dist / vel;
                BatteryLevel cost = travelTime * rate;

                if (cost <= currentBattery) {
                    reachableTasks.push_back(tId);
                }
            }
        }

        // 2. Lógica de decisión fundamental
        if (!pendingTasksExist) {
            return Action(Action::Type::FINISH);
        }

        if (reachableTasks.empty()) {
            return Action(Action::Type::RECHARGE);
        }

        // 3. Elegimos la tarea alcanzable más cercana
        TaskID bestTask = NULL_ID;
        Distance minDistance = std::numeric_limits<Distance>::infinity();

        for (TaskID tId : reachableTasks) {
            NodeID taskNode = scenario->getTasks().at(tId).node;
            Distance d = scenario->distanceBetween(myNode, taskNode);

            if (d < minDistance) {
                minDistance = d;
                bestTask = tId;
            }
        }

        return Action(Action::Type::EXECUTE_TASK, bestTask);
    }
};

} // namespace tau

#endif // TAU_GREEDY_SOLVER_HPP