#ifndef TAU_SCENARIO_HPP
#define TAU_SCENARIO_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

namespace tau {

// Alias de tipos para mayor claridad semántica
using NodeID = int;
using TaskID = int;
using RobotID = int;
using StationID = int;
using Time = double;
using Distance = double;
using BatteryLevel = double;

struct Position {
    double x;
    double y;
};

struct Node {
    NodeID id;
    std::string description;
    Position coords;
    // Mapa de vecinos: <ID del vecino, Distancia Euclidiana>
    std::map<NodeID, Distance> neighbors; 
};

struct TaskInfo {
    TaskID id;
    NodeID node;
    std::string description;
    Time earliestStart;
    Time latestStart;
    double successProb;
    Time averageSuccessTime;
    Time stdSuccessTime;
    Time averageFailTime;
    Time stdFailTime;
    int requiredWorkers;
    int maxAttempts = 1; // Por defecto a 1 si no se especifica
};

struct RobotInfo {
    RobotID id;
    NodeID initialNode;
    std::string description;
    BatteryLevel initialBatteryLevel;
    BatteryLevel batteryCapacity;
    double navigationVelocity;
    double batteryRateWhileNavigating;
    double batteryRateWhileExecuting;
    std::set<TaskID> capabilities;
};

struct StationInfo {
    StationID id;
    NodeID node;
    std::string description;
    // Por ahora vamos a asumir que la carga de un robot es inmediata
    // Time timePU = 1.0; // Tiempo por unidad de recarga (asumimos 1.0 si no se indica)
};

class Scenario {
public:
    // Propiedades globales
    std::string name;
    int height;
    int width;
    Time initialTime;

    // Contenedores de datos
    std::map<NodeID, Node> nodes;
    std::map<TaskID, TaskInfo> tasks;
    std::map<RobotID, RobotInfo> robots;
    std::map<StationID, StationInfo> stations;

    // Método estático para cargar desde el YAML
    static std::shared_ptr<Scenario> loadFromYAML(const std::string& filepath);

    // Getters de conveniencia
    const std::map<NodeID, Node>& getNodes() const { return nodes; }
    const std::map<TaskID, TaskInfo>& getTasks() const { return tasks; }
    const std::map<RobotID, RobotInfo>& getRobots() const { return robots; }
    const std::map<StationID, StationInfo>& getStations() const { return stations; }
};

} // namespace tau

#endif // TAU_SCENARIO_HPP