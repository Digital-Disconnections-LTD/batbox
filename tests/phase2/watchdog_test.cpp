#include "model/Watchdog.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

int main() {
  std::atomic<int> aborted = 0;
  batbox::model::StreamWatchdog watchdog({true, 200, 100}, [&] { ++aborted; });
  watchdog.reset();
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  watchdog.reset();
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  assert(!watchdog.aborted());
  std::this_thread::sleep_for(std::chrono::milliseconds(140));
  assert(watchdog.aborted());
  assert(aborted == 1);
  watchdog.clear();
  watchdog.clear();
}
