#ifndef TAU_SIMULATOR_HPP
#define TAU_SIMULATOR_HPP

#include "scenario.hpp"
#include "logger.hpp"
#include "solver_interface.hpp"
#include <queue>
#include <map>
#include <memory>
#include <vector>

namespace tau {

// --- NUEVO: Enum con los cuatro estados que has propuesto ---
enum class TaskStatus {
    PENDING,
    ASSIGNED,
    COMPLETED,
    FAILED
};

enum class EventType {
    ROBOT_NEEDS_DECISION,
    ROBOT_ARRIVES_AT_TASK,
    TASK_RESOLUTION
};

struct Event {
    Time time;
    EventType type;
    RobotID robotID;
    TaskID taskID;

    bool operator>(const Event& other) const { return time > other.time; }
};

// --- AÑADIR ESTA ESTRUCTURA ---
struct PhysicalTask {
    TaskStatus status = TaskStatus::PENDING; // Se inicializa a PENDING
    int assignedWorkers = 0;                 // Cuántos robots han dicho "yo voy"
    int currentAttempts = 0;
};
// ------------------------------

class DistributedSimulator {
private:
    std::shared_ptr<Scenario> scenario;
    std::shared_ptr<Logger> logger;
    
    std::map<RobotID, std::shared_ptr<ISolver>> solvers;
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> eventQueue;

    // Estado físico absoluto
    std::map<RobotID, NodeID> robotPositions;
    std::map<RobotID, BatteryLevel> robotBatteries;
    std::map<TaskID, std::vector<RobotID>> workersAtTask; // Robots esperando en la tarea

    // --- AÑADIR ESTE MAPA ---
    std::map<TaskID, PhysicalTask> physicalTasks;
    // ------------------------
    
    Time globalTime;

public:
    DistributedSimulator(std::shared_ptr<Scenario> scen, std::shared_ptr<Logger> log);
    void registerSolver(RobotID robotID, std::shared_ptr<ISolver> solver);
    void run();

private:
    void initLoggingAndEvents();
    void handleRobotDecision(Time t, RobotID robotID);
    void handleRobotArrivesAtTask(Time t, RobotID robotID, TaskID taskID);
    void handleTaskResolution(Time t, TaskID taskID);
    
    Time calculateTravelTime(RobotID robotID, NodeID src, NodeID dst);
    void consumeBattery(Time t, RobotID robotID, Time duration, double rate);
};

} // namespace tau

#endif // TAU_SIMULATOR_HPP