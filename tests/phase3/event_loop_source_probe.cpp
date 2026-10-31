#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace batbox;
using namespace batbox::event_loop;
namespace {
Json asJson(const std::vector<std::string>& trace){Json result=Json::array();
  for(const auto& item:trace)result.push_back(item);return result;}
std::vector<std::string> zeroSleep(){
  Loop loop;std::vector<std::string> trace;Deferred<Unit> sleep(loop);
  loop.setTimeout(0,[sleep]{sleep.resolve({});});
  sleep.then([&](const Unit*,const std::exception_ptr&){trace.push_back("sleep");});
  loop.queueMicrotask([&]{trace.push_back("micro");});trace.push_back("sync");
  loop.run();return trace;
}
std::vector<std::string> resolvedRace(){
  Loop loop;std::vector<std::string> trace;
  Deferred<std::string> original(loop),timeout(loop);original.resolve("ok");
  auto timer=loop.setTimeout(50,[timeout]{timeout.reject(
    std::make_exception_ptr(std::runtime_error("boom")));},false);
  auto result=race(loop,std::vector{original,timeout});
  result.then([&](const std::string* value,const std::exception_ptr&){
    timer.cancel();trace.push_back(*value);});
  loop.queueMicrotask([&]{trace.push_back("micro");});trace.push_back("sync");
  loop.run();return trace;
}
std::vector<std::string> rejectedRace(){
  Loop loop;std::vector<std::string> trace;
  Deferred<std::string> pending(loop),timeout(loop);
  auto timer=loop.setTimeout(1,[timeout]{timeout.reject(
    std::make_exception_ptr(std::runtime_error("boom")));},false);
  auto result=race(loop,std::vector{pending,timeout});
  result.then([&](const std::string*,const std::exception_ptr& error){
    timer.cancel();try{std::rethrow_exception(error);}catch(const std::exception& e){
      trace.push_back(e.what());}});
  loop.setTimeout(10,[&]{trace.push_back("guard");});trace.push_back("sync");
  loop.run();return trace;
}
std::vector<std::string> aborted(bool before){
  Loop loop;std::vector<std::string> trace;Deferred<Unit> result(loop);
  if(before)result.resolve({});
  result.then([&](const Unit*,const std::exception_ptr&){trace.push_back("sleep");});
  if(!before)result.resolve({});
  loop.queueMicrotask([&]{trace.push_back("micro");});trace.push_back("sync");
  loop.run();return trace;
}
}
int main(){
  Json output=Json::array();
  output.push_back(asJson(zeroSleep()));output.push_back(asJson(resolvedRace()));
  output.push_back(asJson(rejectedRace()));output.push_back(asJson(aborted(true)));
  output.push_back(asJson(aborted(false)));
  std::cout<<value::stringify(output)<<'\n';
}
