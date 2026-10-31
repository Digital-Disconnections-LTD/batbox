#include "bootstrap/IndigenousCosts.hpp"
#include <chrono>

namespace batbox::bootstrap {
double indigenousNowMs(){
  using namespace std::chrono;
  return static_cast<double>(duration_cast<milliseconds>(
    system_clock::now().time_since_epoch()).count());
}
std::shared_ptr<CostState> makeIndigenousCostState(){
  return std::make_shared<CostState>(initialCostState(indigenousNowMs));
}
}
