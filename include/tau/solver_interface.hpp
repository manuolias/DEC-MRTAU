#ifndef TAU_SOLVER_INTERFACE_HPP
#define TAU_SOLVER_INTERFACE_HPP

#include "scenario.hpp"

namespace tau {

// Tipos de acciones que un robot puede decidir hacer
enum class ActionType {
    IDLE,           // No hacer nada de momento
    NAVIGATE,       // Viajar a un nodo (para esperar o posicionarse)
    EXECUTE_TASK,   // Asignarse a una tarea en el nodo actual o viajar a ella
    RECHARGE,       // Viajar a una estación y recargar
    FINISH          // Ir a una estación y apagar el robot (terminar su misión)
};

// Estructura de la acción devuelta por el Solver
struct Action {
    ActionType type;
    NodeID targetNode; // Hacia dónde va
    TaskID targetTask; // Qué tarea va a hacer (si aplica)
    
    // Constructores de conveniencia
    static Action Idle() { return {ActionType::IDLE, -1, -1}; }
    static Action Finish(NodeID stationNode) { return {ActionType::FINISH, stationNode, -1}; }
    static Action Execute(NodeID node, TaskID task) { return {ActionType::EXECUTE_TASK, node, task}; }
};

// La "Creencia Local" (Omega_i): Lo que el robot percibe del mundo
struct LocalBelief {
    Time currentTime;
    NodeID currentNode;
    BatteryLevel currentBattery;
    std::vector<TaskID> pendingTasks; // <-- NUEVO: Tareas que el robot cree que están libres
};

// La Interfaz pura que todos tus algoritmos heredarán
class ISolver {
public:
    virtual ~ISolver() = default;
    
    // Método principal: dado el estado local, el algoritmo decide la siguiente acción
    virtual Action decideNextAction(const LocalBelief& belief, const std::shared_ptr<Scenario>& scenario) = 0;
    
    // Para el futuro: virtual void receiveMessage(const Message& msg) = 0;
};

} // namespace tau

#endif // TAU_SOLVER_INTERFACE_HPP