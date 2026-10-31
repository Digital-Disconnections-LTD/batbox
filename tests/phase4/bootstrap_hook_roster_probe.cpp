#include "bootstrap/HookRoster.hpp"
#include "instructions/hooks/runtime/HookMatching.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
std::shared_ptr<batbox::instructions::hooks::runtime::HookMatcher>
readMatcher(const Json& data){
  using batbox::instructions::hooks::runtime::HookMatcher;
  auto matcher=std::make_shared<HookMatcher>();
  if(data.contains("matcher")&&!data["matcher"].is_null())
    matcher->matcher=data["matcher"].get<std::string>();
  if(data.contains("pluginRoot")&&!data["pluginRoot"].is_null())
    matcher->pluginRoot=data["pluginRoot"].get<std::string>();
  return matcher;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::HookRosterState hooks;
    bootstrap::FlightTransitions transitions;
    std::shared_ptr<bootstrap::RegisteredHookMap> previous;
    std::shared_ptr<instructions::hooks::runtime::HookMatcher> selected;
    bool previousSet=false,selectedSet=false;
    auto snap=[&](){
      const auto roster=bootstrap::getRegisteredHooks(hooks);
      Json entries=nullptr;
      bool selectedPresent=false;
      if(roster){
        entries=Json::array();
        for(const auto& [event,matchers]:*roster){
          Json list=Json::array();
          for(const auto& matcher:matchers){
            list.push_back({{"matcher",matcher->matcher?
              Json(*matcher->matcher):Json(nullptr)},
              {"pluginRoot",matcher->pluginRoot?
              Json(*matcher->pluginRoot):Json(nullptr)},
              {"hasPluginRoot",matcher->pluginRoot.has_value()}});
            if(matcher==selected)selectedPresent=true;
          }
          entries.push_back(Json::array({event,list}));
        }
      }
      Json out={{"entries",entries},
        {"sameRegistry",previousSet?Json(roster==previous):Json(nullptr)},
        {"selectedStillPresent",selectedSet?Json(selectedPresent):
          Json(nullptr)},
        {"schema",transitions.initJsonSchema?
          *transitions.initJsonSchema:Json(nullptr)}};
      previous=roster;previousSet=true;return out;
    };
    Json trace=Json::array();trace.push_back(snap());
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="register"){
        bootstrap::RegisteredHookMap input;
        for(auto it=op["hooks"].begin();it!=op["hooks"].end();++it){
          bootstrap::RegisteredHookList matchers;
          for(const auto& item:it.value())matchers.push_back(readMatcher(item));
          input.emplace_back(it.key(),std::move(matchers));
        }
        bootstrap::registerHookCallbacks(hooks,input);
      }else if(kind=="capture"){
        const auto event=op["event"].get<std::string>();
        const auto index=op["index"].get<std::size_t>();
        for(const auto& [name,matchers]:*bootstrap::getRegisteredHooks(hooks))
          if(name==event)selected=matchers.at(index);
        selectedSet=true;
      }else if(kind=="clear")bootstrap::clearRegisteredHooks(hooks);
      else if(kind=="clearPlugin")
        bootstrap::clearRegisteredPluginHooks(hooks);
      else if(kind=="schema")
        transitions.initJsonSchema=std::make_shared<Json>(op["value"]);
      else if(kind=="reset")bootstrap::resetSdkInitState(hooks,transitions);
      else throw std::runtime_error("Unknown hook-registry operation");
      trace.push_back(snap());
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
