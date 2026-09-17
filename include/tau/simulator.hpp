#ifndef TAU_SIMULATOR_HPP
#define TAU_SIMULATOR_HPP

#include "scenario.hpp"
#include "logger.hpp"
#include "solver_interface.hpp"
#include "state.hpp"
#include "reward_interface.hpp"
#include "observation.hpp"

#include <queue>
#include <map>
#include <memory>
#include <vector>
#include <chrono>

namespace tau {

// Le asignamos valores enteros explícitos para definir la prioridad.
// En nuestra priority_queue (greater), los valores más pequeños salen PRIMERO.
enum class EventType {
    TASK_END = 0,        // Prioridad 1: Siempre primero en caso de empate temporal
    TASK_START = 1,      // Prioridad 2: Inicio de ejecución
    TASK_EXPIRATION = 2, // Prioridad 3: Caducidad de tareas
    ROBOT_DECISION = 3,  // Prioridad 4: Decisiones físicas 
    PLANNING_UPDATE = 4  // Prioridad 5: Pensamiento en segundo plano    
};

struct Event {
    Time time;
    EventType type;
    RobotID robotID = NULL_ID;
    TaskID taskID = NULL_ID;
    int randomTieBreaker = 0; // Para desempatar decisiones simultáneas
    int payload = 0;          // Para pasar datos ocultos (ej. success = 1, fail = 0) entre eventos

    bool operator>(const Event& other) const {
        // 1. Desempate por tiempo (el menor tiempo va primero)
        if (time != other.time) {
            return time > other.time;
        }
        // 2. Desempate por tipo de evento (si el tiempo es el mismo)
        if (type != other.type) {
            return static_cast<int>(type) > static_cast<int>(other.type);
        }
        // 3. Desempate aleatorio (Si es el MISMO tiempo y el MISMO tipo)
        // Esto garantiza que múltiples ROBOT_DECISION en el mismo ms se ordenen al azar
        return randomTieBreaker > other.randomTieBreaker;
    }
};

class DistributedSimulator {
private:
    std::shared_ptr<const Scenario> scenario;
    std::shared_ptr<Logger> logger;
    std::shared_ptr<RewardFunction> reward;

    // Un cerebro (solver) independiente para cada robot físico
    std::map<RobotID, std::shared_ptr<ISolver>> solvers;
    
    // La "Pizarra Pública" donde se guardan las distribuciones del Dec-MCTS
    std::map<RobotID, Distribution> globalDistributions;

    // std::priority_queue<Event, std::vector<Event>, std::greater<Event>> eventQueue;
    tau::State state;
    Time globalTime;

    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> eventQueue; // Cola de eventos

public:
    DistributedSimulator(std::shared_ptr<const Scenario> scen, std::shared_ptr<Logger> log);
    void registerSolver(RobotID robotID, std::shared_ptr<ISolver> solver);
    void registerReward(std::shared_ptr<RewardFunction> reward);
    void run();
    // Devuelve una referencia constante al estado global al final de la ejecución
    const State& getGlobalState() const { return state; }

private:
    void initLogging();
    void finalLogging(double reward, double computingTime);

    // Genera la observación que puede ver un robot
    Observation generateObservation(RobotID robotID);

    void applyAction(RobotID robotID, const Action& action);
    void simulateFinish(RobotID robotID);
    void simulateRecharge(RobotID robotID);
    void simulateTask(RobotID robotID, TaskID taskID);
    void startTask(TaskID taskID); 
    // outcome: bit 0 = desenlace muestreado (1 = éxito), bit 1 = intento interrumpido
    // por agotamiento de batería. Lo codifica startTask en el payload del TASK_END.
    void endTask(TaskID taskID, int outcome); // Extraído de simulateTask
    void expireTask(TaskID taskID);  // Nuevo evento

    void scheduleRobotDecision(Time t, RobotID id);
    void schedulePlanningUpdate(Time t, RobotID id); // Encola eventos de pensamiento (Para DEC-MCTS)

    Time calculateTravelTime(RobotID robotID, NodeID src, NodeID dst);
    BatteryLevel calculateBatteryConsumption(RobotID robotID, Time duration, BatteryRate rate);
};

} // namespace tau

#endif // TAU_SIMULATOR_HPP