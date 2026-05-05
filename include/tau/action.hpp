#pragma once
#include <ostream>
#include "definitions.hpp"

namespace tau {

class Action {
public:
    enum class Type {
        FINISH,
        // OBSERVE_OUTCOME, // Se puede usar para indicar que el robot está esperando a que acabe una tarea
        RECHARGE,
        EXECUTE_TASK,
        START
        // IDLE // Añadimos IDLE para cuando un robot no puede hacer nada
    };

    Action(Type type) : type(type), target(NULL_ID) {}
    Action(Type type, TaskID target) : type(type), target(target) {}
    
    Type getType() const { return type; }
    TaskID getTarget() const { return target; }
    
    /*
    friend std::ostream& operator<<(std::ostream& os, const Action& action) {
        switch(action.type) {
            case Action::Type::FINISH: return os << "Finish";
            // case Action::Type::OBSERVE_OUTCOME: return os << "Observe_outcome";    
            case Action::Type::RECHARGE: return os << "Recharge";
            case Action::Type::EXECUTE_TASK: return os << "Execute_Task(" << action.target << ")";
            // case Action::Type::IDLE: return os << "Idle";
        }
        return os;
    }
    */

private:
    Type type;
    TaskID target;
};

} // namespace tau