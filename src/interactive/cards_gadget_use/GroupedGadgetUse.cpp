#include "interactive/cards_gadget_use/GroupedGadgetUse.hpp"
#include <stdexcept>
#include <unordered_map>

namespace batbox::interactive::cards_gadget_use {
namespace {
const Json& objectField(const Json& object,const char* key){
  if(!object.is_object())throw std::invalid_argument("Grouped Gadget message object required");
  const auto it=object.find(key);
  if(it==object.end())throw std::invalid_argument(std::string("Grouped Gadget field required: ")+key);
  return *it;
}
std::string stringField(const Json& value,const char* key){
  const auto& field=objectField(value,key);
  if(!field.is_string())throw std::invalid_argument(std::string("Grouped Gadget string field required: ")+key);
  return field.get<std::string>();
}
Json filteredProgress(const MessageLookups& lookups,const std::string& id){
  Json progress=Json::array();
  const auto found=lookups.progressMessagesByGadgetUseID.find(id);
  if(found!=lookups.progressMessagesByGadgetUseID.end())
    for(const auto& ref:found->second)
      if(ref)progress.push_back(*ref);
  return gadgets::filterGadgetProgressMessages(progress);
}
}

Json renderGroupedGadgetUse(const Json& groupedMessage,
                          const gadgets::Gadgets& availableGadgets,
                          const MessageLookups& lookups,
                          const GadgetIDs& inProgressGadgetUseIDs,
                          bool shouldAnimate,const GroupedRenderer& renderGrouped){
  const auto name=stringField(groupedMessage,"toolName");
  const auto* gadget=gadgets::findGadgetByName(availableGadgets,name);
  if(!gadget||!gadget->renderGroupedGadgetUse)return Json();
  if(!renderGrouped)throw std::logic_error("Grouped Gadget renderer import required");
  std::unordered_map<std::string,GroupedResult> resultsByID;
  const auto& results=objectField(groupedMessage,"results");
  if(!results.is_array())throw std::invalid_argument("Grouped results array required");
  for(const auto& resultMessage:results){
    const auto& content=objectField(objectField(resultMessage,"message"),"content");
    if(!content.is_array())throw std::invalid_argument("Grouped result content array required");
    for(const auto& block:content){
      if(block.is_object()&&block.value("type","")=="tool_result"){
        const auto id=stringField(block,"tool_use_id");
        GroupedResult result{block,{}};
        if(resultMessage.contains("toolUseResult"))result.output=resultMessage["toolUseResult"];
        resultsByID[id]=std::move(result);
      }
    }
  }
  const auto& messages=objectField(groupedMessage,"messages");
  if(!messages.is_array())throw std::invalid_argument("Grouped messages array required");
  std::vector<GroupedGadgetUseEntry> data;
  data.reserve(messages.size());
  bool anyInProgress=false;
  for(const auto& message:messages){
    const auto& content=objectField(objectField(message,"message"),"content");
    if(!content.is_array()||content.empty())
      throw std::invalid_argument("Grouped Gadget use content required");
    const auto& block=content.at(0);
    const auto id=stringField(block,"id");
    const bool inProgress=inProgressGadgetUseIDs.contains(id);
    anyInProgress|=inProgress;
    GroupedGadgetUseEntry entry{block,
      lookups.resolvedGadgetUseIDs.contains(id),
      lookups.erroredGadgetUseIDs.contains(id),inProgress,
      filteredProgress(lookups,id),{}};
    const auto result=resultsByID.find(id);
    if(result!=resultsByID.end())entry.result=result->second;
    data.push_back(std::move(entry));
  }
  return renderGrouped(*gadget,data,shouldAnimate&&anyInProgress,availableGadgets);
}
}
