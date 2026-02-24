#ifndef TAU_SOLVER_GREEDY_HPP
#define TAU_SOLVER_GREEDY_HPP

#include "solver_interface.hpp"

namespace tau {

class GreedySolver : public ISolver {
private:
    RobotID myID;

public:
    GreedySolver(RobotID id);
    
    // Sobrescribimos la función de decisión
    Action decideNextAction(const LocalBelief& belief, const std::shared_ptr<Scenario>& scenario) override;
};

} // namespace tau

#endif // TAU_SOLVER_GREEDY_HPP