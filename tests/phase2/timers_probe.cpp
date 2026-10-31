#include "timers/Timers.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <vector>
using batbox::Json;
using namespace batbox::timers;
namespace {
class FakeScheduler final:public TimerScheduler {
public:
  struct Timer {std::function<void()> callback;bool active=true;};
  struct Handle final:TimerHandle {std::shared_ptr<Timer> timer;Json* trace;
    void cancel()override{trace->push_back("cancel");timer->active=false;timer->callback={};}};
  Json trace=Json::array();std::vector<std::shared_ptr<Timer>> timers;
  std::shared_ptr<TimerHandle> schedule(double ms,std::function<void()> callback,bool unref)override{
    auto timer=std::make_shared<Timer>();timer->callback=std::move(callback);timers.push_back(timer);
    trace.push_back("schedule:"+batbox::value::stringify(ms));
    if(unref)trace.push_back("unref");
    auto handle=std::make_shared<Handle>();handle->timer=timer;handle->trace=&trace;return handle;
  }
  void fire(){if(!timers.empty()&&timers[0]->active){timers[0]->active=false;auto callback=std::move(timers[0]->callback);if(callback)callback();}}
};
Json evaluate(const Json& input){
  FakeScheduler scheduler;std::stop_source stop;
  const std::string op=input.at("op").get<std::string>();
  Json result;
  if(op=="sleep"){
    if(input.value("preAbort",false))stop.request_stop();
    SleepOptions options;options.throwOnAbort=input.value("throwOnAbort",false);options.unref=input.value("unref",false);
    if(input.value("customError",false))options.abortError=[&]{scheduler.trace.push_back("customError");return std::make_exception_ptr(std::runtime_error("custom abort"));};
    auto pending=sleep(input.at("ms").get<double>(),stop.get_token(),options,scheduler);
    pending.observe([&](const auto&,const auto& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){result={{"state","rejected"},{"message",e.what()}};}}
      else result={{"state","resolved"},{"value",nullptr}};
      scheduler.trace.push_back("settled");
    });
    if(input.value("abort",false))stop.request_stop();
    if(input.value("fire",false))scheduler.fire();
  }else{
    AsyncResult<Json> original;
    if(input.value("preResolve",false))original.resolve(input.value("value",Json()));
    if(input.value("preReject",false))original.reject(std::make_exception_ptr(std::runtime_error("original rejected")));
    auto pending=withTimeout(original,input.at("ms").get<double>(),"timer elapsed",scheduler);
    pending.observe([&](const auto& value,const auto& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){result={{"state","rejected"},{"message",e.what()}};}}
      else result={{"state","resolved"},{"value",*value}};
      scheduler.trace.push_back("settled");
    });
    if(input.value("resolve",false))original.resolve(input.value("value",Json()));
    if(input.value("reject",false))original.reject(std::make_exception_ptr(std::runtime_error("original rejected")));
    if(input.value("fire",false))scheduler.fire();
    if(input.value("lateResolve",false))original.resolve(input.value("value",Json()));
  }
  return {{"result",result},{"trace",scheduler.trace}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))try{std::cout<<evaluate(Json::parse(line)).dump()<<'\n';}
 catch(const std::exception& e){std::cout<<Json{{"fatal",e.what()}}.dump()<<'\n';}}
