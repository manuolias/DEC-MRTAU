#ifndef TAU_RANDOM_SOLVER_HPP
#define TAU_RANDOM_SOLVER_HPP

#include "solver_interface.hpp"
#include <random>
#include <vector>

namespace tau {

class RandomSolver : public ISolver {
private:
    std::mt19937 gen;

public:
    RandomSolver(unsigned int seed = std::random_device{}()) : gen(seed) {}

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

                // Verificamos si la batería es suficiente estrictamente para llegar
                if (cost <= currentBattery) {
                    reachableTasks.push_back(tId);
                }
            }
        }

        // 2. Lógica de decisión
        if (!pendingTasksExist) {
            // No queda nada por hacer en el mundo
            return Action(Action::Type::FINISH);
        }

        if (reachableTasks.empty()) {
            // Hay tareas, pero no llego a ninguna con mi batería actual
            return Action(Action::Type::RECHARGE);
        }

        // 3. Elegimos una tarea alcanzable al azar
        std::uniform_int_distribution<std::size_t> dist(0, reachableTasks.size() - 1);
        TaskID chosenTask = reachableTasks[dist(gen)];

        return Action(Action::Type::EXECUTE_TASK, chosenTask);
    }
};

} // namespace tau

#endif // TAU_RANDOM_SOLVER_HPP