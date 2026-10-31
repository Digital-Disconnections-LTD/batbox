#pragma once
#include "bootstrap/Costs.hpp"
#include <memory>

namespace batbox::bootstrap {
double indigenousNowMs();
std::shared_ptr<CostState> makeIndigenousCostState();
}
