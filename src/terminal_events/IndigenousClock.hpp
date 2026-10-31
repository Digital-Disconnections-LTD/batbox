#pragma once
#include "terminal_events/Event.hpp"
#include <chrono>

namespace batbox::terminal_events {
class IndigenousClock {
public:
  using Clock=std::chrono::steady_clock;
  explicit IndigenousClock(Clock::time_point processOrigin):
    processOrigin_(processOrigin){}
  static IndigenousClock captureProcessOrigin(){return IndigenousClock(Clock::now());}
  double nowMilliseconds()const;
  MonotonicNow callback()const;
  Clock::time_point processOrigin()const{return processOrigin_;}
private:
  Clock::time_point processOrigin_;
};
} // namespace batbox::terminal_events
