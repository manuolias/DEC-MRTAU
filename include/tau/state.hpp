#pragma once

#include <map>
#include "definitions.hpp" 
#include "action.hpp"
#include "scenario.hpp"

namespace tau {

class State {
private:
    std::shared_ptr<const Scenario> scenario;
    std::map<RobotID, Robot> robots;
    std::map<TaskID, Task> tasks;

public:
    State() = default;
    State(std::shared_ptr<const Scenario> scen);

    const std::map<RobotID, Robot>& getRobots() const { return robots; }
    const std::map<TaskID, Task>& getTasks() const { return tasks; }
    const Scenario& getScenario() const { return *scenario; }

    // Referencias mutables para que el simulador actualice la física
    Robot& getRobot(RobotID id) { return robots.at(id); }
    Task& getTask(TaskID id) { return tasks.at(id); }

    // Siguiente robot a actuar
    // RobotID getNextRobotID() const;

    // Condición de finalización global
    bool isFinal() const;

};

} // namespace tau