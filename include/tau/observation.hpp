#ifndef TAU_OBSERVATION_HPP
#define TAU_OBSERVATION_HPP

#include <vector>
#include <map>
#include "definitions.hpp"

namespace tau {

// --- Tipos de datos preparados para el solver Dec-MCTS ---
// Un Bundle representa una secuencia de tareas planificada.
using Bundle = std::vector<TaskID>; 

// Una Distribución asocia cada ruta (Bundle) con su probabilidad de ser ejecutada (0.0 a 1.0).
using Distribution = std::map<Bundle, double>; 
// ----------------------------------------------------------------

// Información sobre el próximo instante de decisión de un robot: <time, node>
using NextDecisionInfo = std::pair<Time, NodeID>;

class Observation {
private:
    Time currentTime;
    RobotID myId;
    Robot myState;                                // s^{r_i}: Estado propio
    const std::map<TaskID, Task> knownTasks;      // s^T: Estado de las tareas
    const std::map<RobotID, Robot> knownRobots;   // s^R: Estado de los compañeros
    
    // <--- NUEVO: La "pizarra" con las creencias públicas actuales de todos los robots
    const std::map<RobotID, Distribution> knownDistributions; 
    // Mapa con el tiempo estimado en que cada robot quedará libre y el nodo
    const std::map<RobotID, NextDecisionInfo> nextDecisionInfo;

public:
    // Constructor actualizado para recibir la pizarra de distribuciones
        Observation(Time time, RobotID id, const Robot& self, 
                                const std::map<TaskID, Task>& tasks, 
                                const std::map<RobotID, Robot>& robots,
                                const std::map<RobotID, Distribution>& distributions,
                                const std::map<RobotID, NextDecisionInfo>& nextDecisions)
                : currentTime(time), 
                    myId(id), 
                    myState(self), 
                    knownTasks(tasks), 
                    knownRobots(robots), 
                    knownDistributions(distributions),
                    nextDecisionInfo(nextDecisions) {}

    // Getters de solo lectura para los Solvers
    Time getCurrentTime() const { return currentTime; }
    RobotID getId() const { return myId; }
    const Robot& getMyState() const { return myState; }
    const std::map<TaskID, Task>& getKnownTasks() const { return knownTasks; }
    const std::map<RobotID, Robot>& getKnownRobots() const { return knownRobots; }
    
    // <--- NUEVO: Getter para el Dec-MCTS
    const std::map<RobotID, Distribution>& getKnownDistributions() const { return knownDistributions; }

    // <--- NUEVO: Getter para el tiempo y nodo de la siguiente decisión de cada robot
    const std::map<RobotID, NextDecisionInfo>& getNextDecisionInfo() const { return nextDecisionInfo; }
};

} // namespace tau

#endif // TAU_OBSERVATION_HPP