#include "gadgets/bash/AsyncBashCall.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace batbox;
using namespace batbox::bash_gadget;

int main(){
  Json output=Json::object();
  for(const char* mode:{"settled","pending","rejected","throw"}){
    event_loop::Loop loop;
    auto services=std::make_shared<Services>();
    auto host=std::make_shared<AsyncMissionHost>();
    auto events=std::make_shared<Json>(Json::array());
    auto result=std::make_shared<Json>();
    std::optional<event_loop::Deferred<gadgets::GadgetResult>> pending;
    event_loop::Loop::TimerHandle timer;
    host->applySedEdit=[&](const Json&,const gadgets::GadgetCallContext&,const Json&){
      events->push_back("apply");
      if(std::string(mode)=="throw")throw std::runtime_error("sed failed");
      event_loop::Deferred<gadgets::GadgetResult> inner(loop);
      inner.then([events](const gadgets::GadgetResult*,const std::exception_ptr& error){
        events->push_back(error?"inner-error":"inner");
      });
      if(std::string(mode)=="pending")pending=inner;
      else if(std::string(mode)=="rejected")
        inner.reject(std::make_exception_ptr(std::runtime_error("sed failed")));
      else {gadgets::GadgetResult value;value.data={{"stdout","sed"}};inner.resolve(value);}
      return inner;
    };
    gadgets::GadgetCallContext context;context.data=Json::object();
    auto outer=callAsyncBash(loop,services,host,
      Json{{"_simulatedSedEdit",Json{{"filePath","/tmp/example"}}}},context,Json(),{});
    events->push_back("after-call");
    outer.then([events,result](const gadgets::GadgetResult* value,const std::exception_ptr& error){
      if(error){
        events->push_back("outer-error");
        try{std::rethrow_exception(error);}catch(const std::exception& e){*result={{"error",e.what()}};}
      }else {events->push_back("outer");*result={{"data",value->data}};}
    });
    loop.queueMicrotask([events]{events->push_back("sibling");});
    if(std::string(mode)=="pending")
      timer=loop.setTimeout(1,[events,&pending]{
        events->push_back("settle-inner");
        gadgets::GadgetResult value;value.data={{"stdout","sed"}};pending->resolve(value);
      });
    loop.run();
    output[mode]={{"events",*events},{"result",*result}};
  }
  std::cout<<value::stringify(output)<<'\n';
}
