#include "interactive/cards_remaining/AttachmentCard.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_remaining {
namespace {
std::string field(const Json& object,const char* key){
  if(!object.is_object())return {};
  const auto found=object.find(key);
  return found!=object.end()&&found->is_string()?found->get<std::string>():"";
}
void generic(AttachmentCard& card){
  const auto status=field(card.original,"status");
  const auto statusText=status=="completed"?"completed in background":
    status=="killed"?"stopped":status=="running"?
      "still running in background":status;
  const auto description=field(card.original,"description");
  card.kind=AttachmentCard::Kind::Lines;
  card.lines.push_back({{
    {"● Mission \"",false,{},{}},{description,true,{},{}},
    {"\" "+statusText,false,{},{}}},"",true,false,1});
}
}
void classifyAttachmentMissionStatus(AttachmentCard& card,
    const AttachmentServices& services){
  if(!services.batSwarmsEnabled)
    throw std::logic_error("Bat swarms feature import required");
  if(services.batSwarmsEnabled()&&
     field(card.original,"taskType")=="in_process_teammate"){
    if(!services.lookupMission)
      throw std::logic_error("Live Mission state import required");
    const auto mission=services.lookupMission(field(card.original,"taskId"));
    if(mission&&field(*mission,"type")=="in_process_teammate"){
      const auto identity=mission->value("identity",Json::object());
      const auto name=field(identity,"agentName");
      const auto color=field(identity,"color");
      const auto status=field(card.original,"status");
      const auto text=status=="completed"?"shut down gracefully":status;
      card.kind=AttachmentCard::Kind::Lines;
      card.lines.push_back({{
        {"● Partner ",false,{},{}},
        {"@"+name,true,{},color},
        {" "+text,false,{},{}}},"",true,false,1});
      return;
    }
  }
  generic(card);
}
}
