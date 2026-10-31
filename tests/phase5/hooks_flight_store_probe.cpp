#include "instructions/hooks/runtime/FlightHookStore.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace hooks=batbox::instructions::hooks::runtime;
namespace {
Json normalizedHook(const std::shared_ptr<hooks::FlightHookValue>& hook){
  const auto& fields=hook->fields;
  return {{"type",hooks::FlightHookValue{fields,{}}.type()},
    {"id",value::property(fields,"id")},
    {"command",value::property(fields,"command")},
    {"prompt",value::property(fields,"prompt")}};
}
Json normalize(const hooks::FlightDerivedHookMap& values){
  Json result=Json::array();
  for(const auto& [event,matchers]:values){
    Json items=Json::array();
    for(const auto& matcher:matchers){
      Json member={{"matcher",matcher.matcher},
        {"skillRoot",matcher.skillRoot?Json(*matcher.skillRoot):Json()},
        {"hooks",Json::array()}};
      for(const auto& hook:matcher.hooks)
        member["hooks"].push_back(normalizedHook(hook));
      items.push_back(std::move(member));
    }
    result.push_back(Json::array({event,std::move(items)}));
  }
  return result;
}
Json normalize(const hooks::FlightFunctionHookMap& values){
  Json result=Json::array();
  for(const auto& [event,matchers]:values){
    Json items=Json::array();
    for(const auto& matcher:matchers){
      Json member={{"matcher",matcher.matcher},{"skillRoot",nullptr},
        {"hooks",Json::array()}};
      for(const auto& hook:matcher.hooks)
        member["hooks"].push_back(normalizedHook(hook));
      items.push_back(std::move(member));
    }
    result.push_back(Json::array({event,std::move(items)}));
  }
  return result;
}
}
int main(){
  hooks::FlightHooksState state;
  Json trace=Json::array();
  hooks::FlightHookStoreImports imports;
  imports.setAppState=[&](const auto& updater){trace.push_back("set");
    updater(state);};
  imports.logForDebugging=[&](const std::string& text){
    trace.push_back("debug:"+text);};
  imports.dateNow=[&]{trace.push_back("now");return 111.0;};
  imports.random=[&]{trace.push_back("random");return 0.25;};
  imports.defaultHookShell="bash";
  imports.hookEvents={"PreToolUse","Stop","PostToolUse"};
  hooks::FlightHookStore store(std::move(imports));
  auto command=std::make_shared<hooks::FlightHookValue>();
  command->fields={{"type","command"},{"command","echo hi"}};
  auto second=std::make_shared<hooks::FlightHookValue>();
  second->fields={{"type","command"},{"command","echo bye"},{"shell","bash"}};
  auto poetry=std::make_shared<hooks::FlightHookValue>();
  poetry->fields={{"type","prompt"},{"prompt","review"}};
  store.addFlightHook("s","Stop","",command);
  const auto* heldMap=&state;
  const auto heldStore=state.at("s");
  const auto heldMatcher=heldStore->hooks.at("Stop").at(0);
  const auto heldHooks=heldMatcher->hooks;
  store.addFlightHook("s","Stop","",second);
  Json spread={{"mapSame",heldMap==&state},
    {"storeReplaced",heldStore!=state.at("s")},
    {"matcherReplaced",heldMatcher!=state.at("s")->hooks.at("Stop").at(0)},
    {"heldHooks",heldHooks.size()},
    {"firstEntrySame",heldHooks.at(0)==
      state.at("s")->hooks.at("Stop").at(0)->hooks.at(0)},
    {"firstHookSame",heldHooks.at(0)->hook==
      state.at("s")->hooks.at("Stop").at(0)->hooks.at(0)->hook}};
  store.addFlightHook("s","PreToolUse","Bash",poetry,{},"skill");
  auto callback=[](const Json&,std::stop_token)->hooks::FunctionHookReply{
    return true;};
  const auto explicitId=store.addFunctionHook("s","Stop","",callback,
    "must output",1000,"explicit");
  const auto generated=store.addFunctionHook("s","Stop","special",callback,
    "must output");
  const auto function=state.at("s")->hooks.at("Stop").at(0)->hooks.at(2)->hook;
  Json before={{"command",normalize(store.getFlightHooks(state,"s"))},
    {"functions",normalize(store.getFlightFunctionHooks(state,"s"))},
    {"found",static_cast<bool>(store.getFlightHookCallback(state,"s","Stop","",*command))},
    {"functionFound",static_cast<bool>(store.getFlightHookCallback(state,"s","Stop","",*function))}};
  store.removeFunctionHook("s","Stop",explicitId);
  store.removeFlightHook("s","Stop",*second);
  Json after={{"command",normalize(store.getFlightHooks(state,"s","Stop"))},
    {"functions",normalize(store.getFlightFunctionHooks(state,"s","Stop"))}};
  store.clearFlightHooks("s");
  spread["heldStoreHooks"]=heldStore->hooks.at("Stop").at(0)->hooks.size();
  spread["heldMatcherHooks"]=heldMatcher->hooks.size();
  spread["mapAfterClearSame"]=heldMap==&state;
  Json result={{"explicit",explicitId},{"generated",generated},
    {"before",before},{"after",after},
    {"empty",normalize(store.getFlightHooks(state,"s"))},
    {"trace",trace},{"spread",spread}};
  std::cout<<result.dump()<<'\n';
}
