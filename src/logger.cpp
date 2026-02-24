#include "tau/logger.hpp"
#include <iostream>

namespace tau {

Logger::Logger(const std::string& filename) : logFilename(filename) {
    file.open(filename);
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir el archivo de log: " + filename);
    }
}

Logger::~Logger() {
    if (file.is_open()) file.close();
}

void Logger::logInfo(const std::string& key, const std::string& value) {
    file << "info: " << key << "; value: " << value << "\n";
}

void Logger::logStationSpawn(Time t, StationID id, const Position& pos) {
    file << "event: recharging_station_spawn; timestamp: " << t << "; recharging_station: " << id 
         << "; pos: (" << pos.x << ", " << pos.y << ")\n";
}

void Logger::logTaskSpawn(Time t, const TaskInfo& task, const Position& pos) {
    file << "event: task_spawn; timestamp: " << t << "; task: " << task.id 
         << "; pos: (" << pos.x << ", " << pos.y << "); earliest_start: " << task.earliestStart 
         << "; latest_start: " << task.latestStart << "; state: PENDING; required_workers: " 
         << task.requiredWorkers << "; success_prob: " << task.successProb << "; max_attempts: " 
         << task.maxAttempts << "\n";
}

void Logger::logRobotSpawn(Time t, RobotID id, const Position& pos, BatteryLevel cap, double vel, double navRate) {
    file << "event: robot_spawn; timestamp: " << t << "; robot: " << id 
         << "; pos: (" << pos.x << ", " << pos.y << "); state: AVAILABLE; battery_level: " << cap 
         << "; battery_capacity: " << cap << "; velocity: " << vel << "; battery_rate_nav: " << navRate << "\n";
}

void Logger::logBatteryConsumption(Time t, RobotID id, Time duration, BatteryLevel initLvl, BatteryLevel finalLvl) {
    file << "event: battery_consumption; timestamp: " << t << "; robot: " << id 
         << "; time: " << duration << "; initial_level: " << initLvl << "; final_level: " << finalLvl << "\n";
}

void Logger::logNavigation(Time t, RobotID id, Time duration, const Position& src, const Position& dst) {
    file << "event: navigation; timestamp: " << t << "; robot: " << id << "; time: " << duration 
         << "; src: (" << src.x << ", " << src.y << "); dst: (" << dst.x << ", " << dst.y << ")\n";
}

void Logger::logTaskExecution(Time t, TaskID task, RobotID robot, const Position& pos, Time duration) {
    file << "event: task_execution; timestamp: " << t << "; task: " << task << "; robot: " << robot 
         << "; pos: (" << pos.x << ", " << pos.y << "); time: " << duration << "\n";
}

void Logger::logTaskResolution(Time t, TaskID task, const Position& pos, const std::string& status, int attempt) {
    file << "event: task_resolution; timestamp: " << t << "; task: " << task 
         << "; pos: (" << pos.x << ", " << pos.y << "); status: " << status << "; attempt: " << attempt << "\n";
}

void Logger::logRobotFinished(Time t, RobotID id, const Position& pos, BatteryLevel battery) {
    file << "event: robot_finished; timestamp: " << t << "; robot: " << id 
         << "; pos: (" << pos.x << ", " << pos.y << "); battery: " << battery << "\n";
}

} // namespace tau