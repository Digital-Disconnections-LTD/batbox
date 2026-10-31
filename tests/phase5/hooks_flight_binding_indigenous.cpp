#include "instructions/hooks/runtime/FlightHookBinding.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
namespace hooks=batbox::instructions::hooks::runtime;
int main(){
  hooks::FlightHooksState state;
  hooks::FlightHookStoreImports imports;
  imports.setAppState=[&](const auto& update){update(state);};
  imports.logForDebugging=[](const std::string&){};
  imports.dateNow=[](){return 111.0;};
  imports.random=[](){return 0.25;};
  imports.defaultHookShell="bash";
  imports.hookEvents={"Stop"};
  auto store=std::make_shared<hooks::FlightHookStore>(std::move(imports));
  hooks::BatHookServices bat;
  hooks::bindBatHookFlightStore(bat,store,"StructuredOutput");
  bat.registerStructuredOutputEnforcement("hook-agent-1");
  auto functions=store->getFlightFunctionHooks(state,"hook-agent-1","Stop");
  assert(functions.size()==1&&functions.front().second.size()==1);
  auto hook=functions.front().second.front().hooks.front();
  assert(hook->function&&hook->function->timeout==5000);
  Json messages=Json::array({Json{{"type","assistant"},
    {"message",{{"content",Json::array({Json{{"type","tool_use"},
      {"name","StructuredOutput"},{"id","result"}}})}}}},
    Json{{"type","user"},{"message",{{"content",Json::array(
      {Json{{"type","tool_result"},{"tool_use_id","result"}}})}}}}});
  auto reply=hook->function->callback(messages,{});
  assert(std::holds_alternative<bool>(reply)&&std::get<bool>(reply));
  bat.clearFlightHooks("hook-agent-1");
  assert(store->getFlightFunctionHooks(state,"hook-agent-1").empty());
  std::cout<<"agent structured-output enforcement registers and clears native function hook\n";
}
