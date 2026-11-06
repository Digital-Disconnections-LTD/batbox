#include "query/OwnerWatchdogTimer.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using batbox::Json;
namespace {
void check(bool condition, const std::string& what) {
  if (!condition) throw std::runtime_error("check failed: " + what);
}

void watchdogResetsStayOffTheOwner() {
  batbox::event_loop::Loop loop;
  auto hold = loop.hold();
  std::mutex mutex;
  std::condition_variable ready;
  bool aborted = false;
  std::vector<std::string> trace;
  std::atomic<int> scheduled{0};
  auto owner = batbox::query::makeOwnerWatchdogTimer(loop);
  batbox::model::StreamWatchdog watchdog({true, 400, 200}, [&] {
    trace.push_back("abort");
    { std::lock_guard lock(mutex); aborted = true; }
    ready.notify_all();
  }, [&] { trace.push_back("warn"); }, [&] { trace.push_back("timeout"); },
    [&](double delay, std::function<void()> callback) { ++scheduled; return owner(delay, std::move(callback)); });
  std::thread worker([&] {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(600);
    while (std::chrono::steady_clock::now() < until) {
      watchdog.reset();
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::unique_lock lock(mutex);
    if (!ready.wait_for(lock, std::chrono::seconds(10), [&] { return aborted; })) std::terminate();
    lock.unlock();
    watchdog.clear();
    hold.release();
  });
  loop.run();
  worker.join();
  check(watchdog.aborted(), "idle stream aborts");
  check((trace == std::vector<std::string>{"warn", "timeout", "abort"}), "warning, timeout and abort fire once, in order");
  check(scheduled.load() <= 20, "resets do not schedule owner timers (" + std::to_string(scheduled.load()) + " scheduled)");
}
} // namespace

int main() {
  watchdogResetsStayOffTheOwner();
  std::cout << "stream watchdog: resets stay off the owner loop; warning, timeout and abort still fire\n";
  return 0;
}
