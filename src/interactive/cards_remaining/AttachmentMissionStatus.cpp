#include "interactive/cards_remaining/AttachmentCard.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::interactive::cards_remaining {
namespace {
std::string field(const Json& object,const char* key){
  if(!object.is_object())return {};
  const auto found=object.find(key);
  return found!=object.end()&&found->is_string()?found->get<std::string>():"";
}
}
void classifyAttachmentMissionStatus(AttachmentCard& card){
  const auto status=field(card.original,"status");
  const auto statusText=status=="completed"?"completed in background":
    status=="killed"?"stopped":status=="running"?
      "still running in background":status;
  const auto description=field(card.original,"description");
  card.kind=AttachmentCard::Kind::Lines;
  card.lines.push_back({{
    {"● Mission \"",false,{}},{description,true,{}},
    {"\" "+statusText,false,{}}},"",true,false,1});
}
}
