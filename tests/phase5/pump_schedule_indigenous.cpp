#include "terminal_ftxui/PumpSchedule.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace tf = batbox::terminal_ftxui;

int main() {
  tf::PumpSchedule schedule(8, 64, 3);
  assert(schedule.current() == 8 && !schedule.backedOff());
  // A busy stream keeps the short interval indefinitely.
  for (int i = 0; i < 1000; ++i) assert(schedule.next(true) == 8);
  // A short quiet stretch does not slow input polling yet.
  assert(schedule.next(false) == 8);
  assert(schedule.next(false) == 8);
  assert(schedule.next(false) == 8);
  assert(!schedule.backedOff());
  // Afterwards the interval doubles up to the idle ceiling.
  assert(schedule.next(false) == 16);
  assert(schedule.next(false) == 32);
  assert(schedule.next(false) == 64);
  assert(schedule.next(false) == 64);
  assert(schedule.backedOff());
  // Any frame or input returns to the short interval at once.
  assert(schedule.next(true) == 8);
  assert(!schedule.backedOff());
  for (int i = 0; i < 10; ++i) (void)schedule.next(false);
  assert(schedule.backedOff());
  // A redraw request between pumps wakes it too.
  schedule.wake();
  assert(schedule.current() == 8 && !schedule.backedOff());
  assert(schedule.next(false) == 8);

  bool rejected = false;
  try { tf::PumpSchedule invalid(0, 64, 3); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  rejected = false;
  try { tf::PumpSchedule invalid(16, 8, 3); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  std::cout << "terminal pump keeps 8 ms while active and backs off when idle\n";
  return 0;
}
