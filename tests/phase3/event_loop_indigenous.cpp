#include "event_loop/Deferred.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>
using namespace batbox::event_loop;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  try{
    {
      Loop loop;std::vector<std::string> trace;
      Deferred<int> resolved(loop);resolved.resolve(7);
      resolved.then([&](const int* value,const std::exception_ptr&){
        trace.push_back("then:"+std::to_string(*value));
        loop.queueMicrotask([&]{trace.push_back("nested");});});
      loop.queueMicrotask([&]{trace.push_back("micro");});
      loop.postMission([&]{trace.push_back("task");});
      trace.push_back("sync");loop.run();
      check(trace==std::vector<std::string>{"sync","then:7","micro","nested","task"},
        "settled then/microtask checkpoint order wrong");
    }
    {
      Loop loop;std::vector<std::string> trace;
      Deferred<int> first(loop),second(loop);first.resolve(1);second.resolve(2);
      auto winner=race(loop,std::vector{first,second});
      winner.then([&](const int* value,const std::exception_ptr&){
        trace.push_back("winner:"+std::to_string(*value));});
      trace.push_back("sync");loop.run();
      check(trace==std::vector<std::string>{"sync","winner:1"},
        "already-settled Promise.race did not use input order");
    }
    {
      Loop loop;bool fired=false;
      loop.setTimeout(50,[&]{fired=true;},false);
      const auto begin=std::chrono::steady_clock::now();loop.run();
      const auto elapsed=std::chrono::steady_clock::now()-begin;
      check(!fired&&elapsed<std::chrono::milliseconds(30),
        "unref timer kept loop alive");
      loop.setTimeout(60,[]{},true);loop.run();
      check(fired,"unref timer did not fire while a referenced timer kept loop alive");
    }
    {
      Loop loop;bool fired=false;
      auto timer=loop.setTimeout(1,[&]{fired=true;});timer.cancel();loop.run();
      check(!fired,"cancelled timer fired");
      check(Loop::normalizeTimeout(0)==1&&Loop::normalizeTimeout(-5)==1&&
        Loop::normalizeTimeout(2.9)==2&&Loop::normalizeTimeout(2147483648.0)==1,
        "Node timeout normalization changed");
    }
    {
      Loop loop;std::vector<std::string> trace;
      auto guard=loop.setTimeout(300,[&]{trace.push_back("guard");});
      std::thread producer([dispatcher=loop.dispatcher(),guard,&trace]{
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        dispatcher.postMission([dispatcher,guard,&trace]{
          check(dispatcher.isOwnerThread(),"task ran off owner strand");
          trace.push_back("posted");guard.cancel();});
      });
      loop.run();producer.join();
      check(trace==std::vector<std::string>{"posted"},
        "cross-thread post did not wake owner or cancel referenced timer");
    }
    {
      Loop loop;std::vector<std::string> trace;
      loop.setTimeout(1,[&]{trace.push_back("timer");
        loop.postMission([&]{trace.push_back("posted-after-last-timer");});});
      loop.run();
      check(trace==std::vector<std::string>{"timer","posted-after-last-timer"},
        "post from last referenced callback was dropped");
    }
    {
      Loop loop;bool fired=false;
      loop.setTimeout(1,[&]{fired=true;},false);
      std::this_thread::sleep_for(std::chrono::milliseconds(3));loop.runReady();
      check(fired,"runReady did not pump an unreferenced timer when explicitly requested");
    }
    {
      Loop loop;auto lease=loop.hold();
      check(lease.active(),"new keepalive not referenced");
      const auto begin=std::chrono::steady_clock::now();
      std::thread worker([lease=std::move(lease)]()mutable{
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
        lease.release(); // foreign release posts close to the owner
      });
      loop.run();worker.join();
      check(std::chrono::steady_clock::now()-begin>=std::chrono::milliseconds(10),
        "keepalive did not hold loop until worker completion");
    }
    {
      std::promise<void> ownerClosed;
      auto gate=ownerClosed.get_future().share();
      std::thread lateRelease;
      {
        Loop loop;auto lease=loop.hold();
        lateRelease=std::thread([gate,lease=std::move(lease)]()mutable{
          gate.wait();lease.release();
        });
        // Destruction closes an outstanding referenced handle on the owner.
      }
      ownerClosed.set_value();lateRelease.join();
    }
    {
      Loop loop;std::vector<std::string> trace;Deferred<int> done(loop);
      auto guard=loop.setTimeout(300,[&]{trace.push_back("guard");});
      done.then([&](const int* value,const std::exception_ptr&){
        check(loop.isOwnerThread(),"foreign Deferred resolution ran observer off owner");
        trace.push_back(std::to_string(*value));guard.cancel();});
      std::thread producer([done]{std::this_thread::sleep_for(std::chrono::milliseconds(5));
        done.resolve(9);});
      loop.run();producer.join();
      check(trace==std::vector<std::string>{"9"},
        "foreign Promise resolution did not post owner microtask");
    }
    {
      Loop loop;loop.setTimeout(1,[]{throw std::runtime_error("owner callback failed");});
      bool surfaced=false;
      try{loop.run();}catch(const std::runtime_error& error){
        surfaced=std::string(error.what())=="owner callback failed";}
      check(surfaced,"C callback exception was not returned to loop owner");
    }
    std::cout<<"PASS libuv owner strand, microtasks, race, timers, unref, cross-thread wake\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
