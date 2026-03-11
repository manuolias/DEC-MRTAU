#pragma once

#include <algorithm>
#include <map>
#include "state.hpp"
#include "logger.hpp"
#include "utils/svector.hpp"


namespace tau {

class RewardFunction {
public:
    virtual ~RewardFunction() = default;
    virtual double getValue(const State& state) const = 0;

   
};

} // namespace tau