#ifndef TAU_OBSERVATION_HPP
#define TAU_OBSERVATION_HPP

#include <vector>
#include <map>
#include <string>
#include "definitions.hpp"

namespace tau {

enum MessageType {
    BID_REQUEST,
    BID_REPLY,
    // Otros tipos de mensajes que puedas necesitar
};

// Estructura básica para la comunicación entre robots (\mathcal{M}_{in})
struct Message {
    RobotID sender;
    RobotID receiver; // Puede ser un ID específico o un broadcast (-1)
    MessageType type;
    TaskID targetTask = NULL_ID;
    double value = 0.0;           // Servirá para la puja (bid)
    // std::vector<double> data;  // <-- Puedes descomentar esto cuando hagas CBBA
};

class Observation {
private:
    Time currentTime; // <--- NUEVO
    RobotID myId;
    Robot myState;                                // s^{r_i}: Estado propio
    const std::map<TaskID, Task> knownTasks;            // s^T: Estado de las tareas
    const std::map<RobotID, Robot> knownRobots;          // s^R: Estado de los robots (opcional, dependiendo de tu diseño)

public:
    Observation(Time time, RobotID id, Robot& self, const std::map<TaskID, Task>& tasks, const std::map<RobotID, Robot>& robots)
        : currentTime(time), myId(id), myState(self), knownTasks(tasks), knownRobots(robots) {}

    // Getters de solo lectura para el Solver
    Time getCurrentTime() const { return currentTime; }
    RobotID getId() const { return myId; }
    const Robot& getMyState() const { return myState; }
    const std::map<TaskID, Task>& getKnownTasks() const { return knownTasks; }
    const std::map<RobotID, Robot>& getKnownRobots() const { return knownRobots; }
};

} // namespace tau

#endif // TAU_OBSERVATION_HPP