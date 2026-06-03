//
// Created by greenhand520 on 2026/6/3.
//

#pragma once

#include <cmath>

inline double round_to(const double val, const int n) {
    const double factor = std::pow(10.0, n);
    return std::round(val * factor) / factor;
}