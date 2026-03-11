#pragma once

#include "reward_interface.hpp"
#include <stdexcept>

namespace tau {

class RewardFunction00 : public RewardFunction {
public:
    double k1, k2, k3, k4, k5, k6, k7, k8;

    RewardFunction00(double w1=1.0, double w2=0.0, double w3=0.0, double w4=0.0, 
                     double w5=0.0, double w6=0.0, double w7=0.0, double w8=0.0) 
        : k1(w1), k2(w2), k3(w3), k4(w4), k5(w5), k6(w6), k7(w7), k8(w8) {
        
        double sum = k1+k2+k3+k4+k5+k6+k7+k8;
        if (std::abs(1.0-sum)>1e-9) throw std::runtime_error("reward00 weights do not sum 1.0");
    }

    double getValue(const State& state) const override {
        const auto& scenario = state.getScenario();
        const auto& robots = state.getRobots();
        const auto& tasks = state.getTasks();

        double completedTasks = static_cast<double>(getNumberOfTasks(tasks, TaskStatus::COMPLETED));
        double pendingTasks = static_cast<double>(getNumberOfTasks(tasks, TaskStatus::PENDING));
        double failedTasks = static_cast<double>(getNumberOfTasks(tasks, TaskStatus::FAILED));
        double numberOfTasks = static_cast<double>(tasks.size());

        double availableRobots = static_cast<double>(getNumberOfRobots(robots, RobotStatus::AVAILABLE));
        double finishedRobots = static_cast<double>(getNumberOfRobots(robots, RobotStatus::FINISHED));
        double failedRobots = static_cast<double>(getNumberOfRobots(robots, RobotStatus::FAILED));
        double numberOfRobots = static_cast<double>(robots.size());
        
        double a1 = completedTasks / numberOfTasks;
        double a2 = 1.0 - (pendingTasks / numberOfTasks);
        double a3 = 1.0 - (failedTasks / numberOfTasks);

        double a4 = availableRobots / numberOfRobots;
        double a5 = finishedRobots / numberOfRobots;
        double a6 = 1.0 - (failedRobots / numberOfRobots);
       
        double a7 = 1.0 - getMaxMakespanRatio(robots, scenario);
        double a8 = 1.0 - getSumTravelDistanceRatio(robots, scenario);
       
        return k1*a1 + k2*a2 + k3*a3 + k4*a4 + k5*a5 + k6*a6 + k7*a7 + k8*a8;     
    }
};

} // namespace tau