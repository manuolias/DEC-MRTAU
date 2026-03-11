#pragma once

#include "scenario.hpp"
#include <algorithm>
#include <map>

namespace tau {

// Definimos un tipo genérico ID para target (TaskID o StationID)
using ID = int;
const ID NULL_ID = -1;

enum class RobotStatus {
    AVAILABLE,
    FINISHED,
    WAITING,
    EXECUTING,
    FAILED
};

enum class TaskStatus {
    PENDING,
    ASSIGNED,
    EXECUTING,
    COMPLETED,
    FAILED
};

// El estado dinámico de un robot en la simulación
struct Robot {
    RobotStatus status;
    BatteryLevel batteryLevel;
    Time time; // Tiempo en el que el que el robot quedara libre
    NodeID node;
    TaskID onTask; 
    unsigned completedTasks;
    Distance travelDistance;
    bool failed;
};

// El estado dinámico de una tarea en la simulación
struct Task {
    TaskStatus status;
    int assignedWorkers;
    Time initTime;
    Time finalTime;
    int attempts;
};

// Funciones estáticas de cálculo extraídas de tu código
static double getMakespanRatio(RobotID robotID, const Robot& robot, const Scenario& scenario) {
    // Ajusta el horizonte temporal máximo según tu problema
    double total = 1080.0; // Puesto a mano como ejemplo
    if (total <= 0) return 0.0;
    
    double elapsed = robot.time - scenario.initialTime;
    return std::clamp(elapsed / total, 0.0, 1.0);
}

static double getMaxMakespanRatio(const std::map<RobotID, Robot>& robots, const Scenario& scenario) {
    double maxRatio = 0.0;
    for (const auto& [id, robot] : robots) {
        double ratio = getMakespanRatio(id, robot, scenario);
        if (ratio > maxRatio) maxRatio = ratio;
    }
    return maxRatio;
}

static double getSumTravelDistanceRatio(const std::map<RobotID, Robot>& robots, const Scenario& scenario) {
    Distance totalDistance = 0.0;
    Distance maxTotalDistance = 10000.0 * robots.size(); // Ajustar según grafo
    
    for (const auto& [id, robot] : robots) {
        totalDistance += robot.travelDistance;
    }   
    return std::clamp(totalDistance / maxTotalDistance, 0.0, 1.0);
}

static unsigned getNumberOfTasks(const std::map<TaskID, Task>& tasks, TaskStatus status) {
    unsigned count = 0;
    for (const auto& [id, task] : tasks) {
        if (task.status == status) count++;
    }
    return count;
}

static unsigned getNumberOfRobots(const std::map<RobotID, Robot>& robots, RobotStatus status) {
    unsigned count = 0;
    for (const auto& [id, robot] : robots) {
        if (robot.status == status) count++;
    }
    return count;
}

} // namespace tau