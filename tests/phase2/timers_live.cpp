#include "timers/Timers.hpp"
#include <chrono>
#include <iostream>
#include <thread>
using namespace batbox::timers;
int main(){
  RealTimerScheduler scheduler;
  std::stop_source stop;
  auto start=std::chrono::steady_clock::now();
  auto pending=sleep(30000,stop.get_token(),{},scheduler);
  std::this_thread::sleep_for(std::chrono::milliseconds(15));
  stop.request_stop();pending.wait();
  auto elapsed=std::chrono::steady_clock::now()-start;
  if(elapsed>std::chrono::seconds(2)){std::cerr<<"abort did not wake sleep";return 1;}
  AsyncResult<int> underlying;
  auto timed=withTimeout(underlying,10,"timer elapsed",scheduler);
  bool timeout=false;
  try{timed.wait();}catch(const std::runtime_error& e){timeout=std::string(e.what())=="timer elapsed";}
  if(!timeout||underlying.settled()){std::cerr<<"timeout race cancelled work or failed";return 1;}
  underlying.resolve(42);
  if(underlying.wait()!=42){std::cerr<<"underlying work did not finish";return 1;}
  std::cout<<"PASS real abort and timeout\n";
}
