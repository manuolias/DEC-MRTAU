#include "tau/state.hpp"
#include <limits>
#include <vector>
#include <random>

namespace tau {

State::State(std::shared_ptr<const Scenario> scen) : scenario(scen) {

    // Inicializar el estado físico de los robots
    for (const auto& [id, info] : scenario->getRobots()) {
        Robot robot;
        robot.status = RobotStatus::AVAILABLE;
        robot.batteryLevel = info.initialBatteryLevel;
        robot.time = scenario->initialTime;
        robot.node = info.initialNode;
        robot.onTask = NULL_ID; 
        robot.completedTasks = 0;
        robot.travelDistance = 0.0;
        robot.failed = false;
        
        robots[id] = robot;
    }

    // Inicializar el estado físico de las tareas
    for (const auto& [id, info] : scenario->getTasks()) {
        Task task;
        task.status = TaskStatus::PENDING;
        task.assignedWorkers = 0;
        task.initTime = scenario->initialTime;
        task.finalTime = scenario->initialTime;
        task.attempts = 0;
        
        tasks[id] = task;
    }
}

bool State::isFinal() const {
    // La simulación termina si TODOS los robots han FAILED o FINISHED
    for (const auto& [id, robot] : robots) {
        if (robot.status != RobotStatus::FAILED && 
            robot.status != RobotStatus::FINISHED) {
            return false;
        }
    }
    return true;
}

}