#include "instructions/hooks/runtime/ConcurrentAll.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json stepJson(const hook::GeneratorStep<Json>& step){
  Json value={{"done",step.done}};
  if(step.value)value["value"]=*step.value;
  return value;
}
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),values=Json::array(),error=nullptr;
  std::vector<hook::GeneratorNext<Json>> generators;
  std::size_t index=0;
  for(const auto& rows:item.at("generators")){
    const auto id=index++;
    generators.push_back([&,steps=rows,id,position=std::size_t{0}]()mutable{
      trace.push_back("next:"+std::to_string(id)+":"+std::to_string(position));
      event_loop::Deferred<hook::GeneratorStep<Json>> output(loop);
      if(position>=steps.size())output.resolve({true,std::nullopt});
      else{
        const auto step=steps.at(position++);
        if(step.is_object()&&step.contains("error"))
          output.reject(std::make_exception_ptr(
            std::runtime_error(step.at("error").get<std::string>())));
        else if(step.is_object()&&step.contains("delay")){
          const auto remaining=std::make_shared<std::size_t>(
            step.at("delay").get<std::size_t>());
          auto later=std::make_shared<std::function<void()>>();
          const std::weak_ptr<std::function<void()>> weak=later;
          *later=[&,remaining,weak,output,value=step.at("value")](){
            if((*remaining)--){
              if(auto self=weak.lock())loop.queueMicrotask([self]{(*self)();});
              return;
            }
            output.resolve({false,value});
          };
          loop.queueMicrotask([later]{(*later)();});
        }
        else if(step.is_object()&&step.value("undefined",false))
          output.resolve({false,std::nullopt});
        else output.resolve({false,step});
      }
      return output;
    });
  }
  const double cap=item.contains("cap")?item.at("cap").get<double>():
    std::numeric_limits<double>::infinity();
  hook::ConcurrentAll<Json> all(loop,std::move(generators),cap);
  const auto kind=item.value("kind",std::string{});
  std::function<void()> next;
  auto saveError=[&](const std::exception_ptr& failure){
    try{std::rethrow_exception(failure);}
    catch(const std::exception& exception){error=exception.what();}
  };
  if(kind=="return-before"){
    all.returnValue(Json("R")).then([&](const hook::GeneratorStep<Json>* step,
      const std::exception_ptr& failure){
      if(failure){saveError(failure);return;}
      values.push_back(stepJson(*step));
      all.next().then([&](const hook::GeneratorStep<Json>* nextStep,
        const std::exception_ptr& nextFailure){
        if(nextFailure)saveError(nextFailure);
        else values.push_back(stepJson(*nextStep));
      });
    });
  }else if(kind=="return-after"||kind=="throw-after"){
    all.next().then([&](const hook::GeneratorStep<Json>* step,
      const std::exception_ptr& failure){
      if(failure){saveError(failure);return;}
      values.push_back(stepJson(*step));
      auto control=kind=="return-after"?all.returnValue(Json("R")):
        all.throwError(std::make_exception_ptr(std::runtime_error("stop")));
      control.then([&](const hook::GeneratorStep<Json>* controlStep,
        const std::exception_ptr& controlFailure){
        if(controlFailure)saveError(controlFailure);
        else values.push_back(stepJson(*controlStep));
        all.next().then([&](const hook::GeneratorStep<Json>* nextStep,
          const std::exception_ptr& nextFailure){
          if(nextFailure)saveError(nextFailure);
          else values.push_back(stepJson(*nextStep));
        });
      });
    });
  }else if(kind=="queued-return"||kind=="queued-throw"){
    auto first=all.next();
    auto control=kind=="queued-return"?all.returnValue(Json("R")):
      all.throwError(std::make_exception_ptr(std::runtime_error("stop")));
    first.then([&](const hook::GeneratorStep<Json>* step,
      const std::exception_ptr& failure){
      if(failure)saveError(failure);
      else values.push_back(stepJson(*step));
    });
    control.then([&](const hook::GeneratorStep<Json>* step,
      const std::exception_ptr& failure){
      if(failure)saveError(failure);
      else values.push_back(stepJson(*step));
      all.next().then([&](const hook::GeneratorStep<Json>* nextStep,
        const std::exception_ptr& nextFailure){
        if(nextFailure)saveError(nextFailure);
        else values.push_back(stepJson(*nextStep));
      });
    });
  }else{
    next=[&]{all.next().then([&](const hook::GeneratorStep<Json>* step,
      const std::exception_ptr& failure){
      if(failure){saveError(failure);return;}
      if(step->done)return;
      values.push_back(*step->value);next();
    });};
    next();
  }
  loop.run();
  return {{"name",item.at("name")},{"trace",trace},{"values",values},
    {"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
