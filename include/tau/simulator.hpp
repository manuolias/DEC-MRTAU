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
    TASK_RESOLUTION = 0,  // Prioridad 1: Siempre primero en caso de empate temporal
    TASK_EXPIRATION = 1,  // Prioridad 2
    ROBOT_DECISION = 2    // Prioridad 3: Los robots actúan viendo el estado finalizado
};

struct Event {
    Time time;
    EventType type;
    RobotID robotID = NULL_ID;
    TaskID taskID = NULL_ID;
    int randomTieBreaker = 0; // NUEVO: Para desempatar decisiones simultáneas

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

    // NUEVO: Un cerebro (solver) independiente para cada robot físico
    std::map<RobotID, std::shared_ptr<ISolver>> solvers;
    
    // NUEVO: Gestor/Buzón central de mensajes en tránsito
    std::vector<Message> messageBus;

    // std::priority_queue<Event, std::vector<Event>, std::greater<Event>> eventQueue;
    tau::State state;
    Time globalTime;

public:
    DistributedSimulator(std::shared_ptr<const Scenario> scen, std::shared_ptr<Logger> log);
    void registerSolver(RobotID robotID, std::shared_ptr<ISolver> solver);
    void registerReward(std::shared_ptr<RewardFunction> reward);
    void run();
    // Devuelve una referencia constante al estado global al final de la ejecución
    const State& getGlobalState() const { return state; }

private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> eventQueue; // Cola de eventos

    void initLogging();
    void finalLogging(double reward, double computingTime);

    // NUEVO: Función clave que "filtra" la realidad para crear la Observación
    Observation generateObservation(RobotID robotID);
    
    // NUEVO: Fase de enrutamiento de comunicaciones
    void routeMessages();

    void applyAction(RobotID robotID, const Action& action);
    void simulateFinish(RobotID robotID);
    void simulateRecharge(RobotID robotID);
    void simulateTask(RobotID robotID, TaskID taskID);
    void resolveTask(TaskID taskID); // Extraído de simulateTask
    void expireTask(TaskID taskID);  // Nuevo evento

    void scheduleRobotDecision(Time t, RobotID id);
    
    Time calculateTravelTime(RobotID robotID, NodeID src, NodeID dst);
    BatteryLevel calculateBatteryConsumption(RobotID robotID, Time duration, BatteryRate rate);
};

} // namespace tau

#endif // TAU_SIMULATOR_HPP