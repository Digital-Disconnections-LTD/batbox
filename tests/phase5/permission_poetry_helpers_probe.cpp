#include "cli/permissions/PoetryHelpers.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array();
    const auto reason=cli::permissions::serializeDecisionReason(
      value::property(input,"reason"),[&](const std::string& name){
        events.push_back(Json::array({"feature",name}));
        return input.value(name,false);
      });
    gadgets::Gadget gadget;gadget.name="Read";
    gadget.userFacingName=[&](const Json&){
      events.push_back(Json::array({"facing"}));
      if(input.value("throwFacing",false))
        throw std::runtime_error("facing failed");
      const auto& value=value::property(input,"facing");
      return value.is_string()?value.get<std::string>():"Read a file";
    };
    if(input.contains("activity")||input.value("throwActivity",false))
      gadget.getActivityDescription=[&](const Json&){
        events.push_back(Json::array({"activity"}));
        if(input.value("throwActivity",false))
          throw std::runtime_error("activity failed");
        return value::property(input,"activity");
      };
    if(input.contains("summary")||input.value("throwSummary",false))
      gadget.getGadgetUseSummary=[&](const Json&){
        events.push_back(Json::array({"summary"}));
        if(input.value("throwSummary",false))
          throw std::runtime_error("summary failed");
        return value::property(input,"summary");
      };
    const auto details=cli::permissions::buildRequiresActionDetails(gadget,
      Json{{"file","a"}},"tool-1","request-1");
    std::cout<<value::stringify(Json{{"reason",reason?Json(*reason):Json()},
      {"details",details},{"events",events}})<<'\n';
  }
}
