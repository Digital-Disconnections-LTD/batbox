#include "terminal_events/IndigenousClock.hpp"
#include <cassert>
#include <chrono>

using namespace batbox::terminal_events;
int main(){
  const auto origin=IndigenousClock::Clock::now()-std::chrono::seconds(5);
  const IndigenousClock clock(origin);
  const auto callback=clock.callback();
  const double before=callback();
  TerminalEvent event("keydown",callback);
  const double after=callback();
  assert(before>=5000.0);
  assert(event.timeStamp()>=before);
  assert(after>=event.timeStamp());
  const auto fresh=IndigenousClock::captureProcessOrigin();
  assert(fresh.callback()()>=0.0);
}
