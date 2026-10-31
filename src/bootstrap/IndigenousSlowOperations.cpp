#include "bootstrap/SlowOperations.hpp"
#include <chrono>

namespace batbox::bootstrap {
double indigenousSlowClockMillis(){
  const auto now=std::chrono::system_clock::now().time_since_epoch();
  return static_cast<double>(std::chrono::duration_cast<
    std::chrono::milliseconds>(now).count());
}
}
