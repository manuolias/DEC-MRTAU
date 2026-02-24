#ifndef TAU_LOGGER_HPP
#define TAU_LOGGER_HPP

#include "scenario.hpp"
#include <string>
#include <fstream>

namespace tau {

class Logger {
private:
    std::ofstream file;
    std::string logFilename; // <-- NUEVO: Variable para guardar el nombre

public:
    Logger(const std::string& filename);
    ~Logger();

    // <-- NUEVO: Función para que otros puedan preguntar el nombre
    std::string getFilename() const { return logFilename; }
    
    void logInfo(const std::string& key, const std::string& value);
    void logStationSpawn(Time t, StationID id, const Position& pos);
    void logTaskSpawn(Time t, const TaskInfo& task, const Position& pos);
    void logRobotSpawn(Time t, RobotID id, const Position& pos, BatteryLevel cap, double vel, double navRate);
    void logBatteryConsumption(Time t, RobotID id, Time duration, BatteryLevel initLvl, BatteryLevel finalLvl);
    void logNavigation(Time t, RobotID id, Time duration, const Position& src, const Position& dst);
    void logTaskExecution(Time t, TaskID task, RobotID robot, const Position& pos, Time duration);
    void logTaskResolution(Time t, TaskID task, const Position& pos, const std::string& status, int attempt);
    void logRobotFinished(Time t, RobotID id, const Position& pos, BatteryLevel battery);
};

} // namespace tau

#endif // TAU_LOGGER_HPP