#ifndef TAU_OBSERVATION_HPP
#define TAU_OBSERVATION_HPP

#include <vector>
#include <map>
#include <string>
#include "definitions.hpp"

namespace tau {

// Estructura básica para la comunicación entre robots (\mathcal{M}_{in})
struct Message {
    RobotID sender;
    RobotID receiver; // Puede ser un ID específico o un broadcast (-1)
    std::string type; // ej. "BID", "PLAN_DISTRIBUTION"
    // Aquí en el futuro puedes añadir un payload (datos binarios, JSON, etc.)
};

class Observation {
private:
    RobotID myId;
    Robot myState;                                // s^{r_i}: Estado propio
    const std::map<TaskID, Task> knownTasks;            // s^T: Estado de las tareas
    std::vector<Message> inbox;                   // \mathcal{M}_{in}: Mensajes recibidos

public:
    Observation(RobotID id, Robot& self, const std::map<TaskID, Task>& tasks, const std::vector<Message>& messages)
        : myId(id), myState(self), knownTasks(tasks), inbox(messages) {}

    // Getters de solo lectura para el Solver
    RobotID getId() const { return myId; }
    const Robot& getMyState() const { return myState; }
    const std::map<TaskID, Task>& getKnownTasks() const { return knownTasks; }
    const std::vector<Message>& getInbox() const { return inbox; }
};

} // namespace tau

#endif // TAU_OBSERVATION_HPP