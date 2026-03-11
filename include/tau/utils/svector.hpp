#pragma once

#include <algorithm>
#include <vector>
#include <numeric>
#include <limits>
#include <cmath>
#include <functional>

namespace utils
{
class SVector : public std::vector<double>
{
public:
    using std::vector<double>::vector;
    
    SVector() = default;

    virtual ~SVector() {}

    double max() const {
        auto it = std::max_element(begin(),end());
        return it == end() ? -std::numeric_limits<double>::infinity() : *it;
    }

    double min() const {
        auto it = std::min_element(begin(),end());
        return it == end() ? std::numeric_limits<double>::infinity() : *it;
    }

    double sum() const {
        return std::accumulate(begin(),end(),0.0);
    }

    double mean() const {
        if (empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return sum()/(double)size();
    }

    double sd() const {
        if (size()<2) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        double mu = mean();
        double s = 0;
        for (auto it = begin(); it != end(); ++it) {
            s += (*it - mu) * (*it - mu);
        }
        return std::sqrt(s / (double)(size()-1));
    }

};

}