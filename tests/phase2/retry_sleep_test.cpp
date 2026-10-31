#include "retry/Retry.hpp"
#include <chrono>
#include <iostream>
#include <stop_token>
#include <thread>
using namespace batbox::retry;
using namespace std::chrono;
int main() {
  bool preaborted=false, interrupted=false, normal=false;
  std::stop_source first; first.request_stop();
  try { sleepAbortResponsive(5000,first.get_token()); }
  catch (const RetryFailure& e) { preaborted=e.error.kind==ErrorRecord::Kind::Abort; }
  std::stop_source second;
  std::jthread stopper([&] { std::this_thread::sleep_for(10ms); second.request_stop(); });
  const auto start=steady_clock::now();
  try { sleepAbortResponsive(5000,second.get_token()); }
  catch (const RetryFailure& e) { interrupted=e.error.kind==ErrorRecord::Kind::Abort; }
  const auto elapsed=duration_cast<milliseconds>(steady_clock::now()-start).count();
  try { sleepAbortResponsive(2,{}); normal=true; } catch (...) {}
  if (!preaborted || !interrupted || !normal || elapsed>=1000) return 1;
  std::cout << "PASS preaborted, interrupted in " << elapsed << " ms, normal timer\n";
}
