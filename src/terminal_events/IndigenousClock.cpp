#include "terminal_events/IndigenousClock.hpp"

namespace batbox::terminal_events {
double IndigenousClock::nowMilliseconds()const{
  return std::chrono::duration<double,std::milli>(
    Clock::now()-processOrigin_).count();
}
MonotonicNow IndigenousClock::callback()const{
  const IndigenousClock value=*this;
  return [value]{return value.nowMilliseconds();};
}
} // namespace batbox::terminal_events
