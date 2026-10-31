#pragma once
#include "interactive/message_view/RowPlan.hpp"
#include <algorithm>
namespace message_view_fixture {
using namespace batbox::interactive;
using namespace batbox::interactive::message_view;
using batbox::Json;
inline const Json& field(const Json& value,const char* key){
  static const Json absent(Json::value_t::discarded);
  if(!value.is_object())return absent;
  const auto found=value.find(key);return found==value.end()?absent:*found;
}
inline void put(Json& out,const char* key,const Json& value){if(!value.is_discarded())out[key]=value;}
inline Json node(const char* kind,Json props=Json::object(),Json children=Json::array()){
  return {{"type",kind},{"props",std::move(props)},{"children",std::move(children)}};
}
inline Json idsJson(const GadgetIDs& ids){
  std::vector<std::string> ordered(ids.begin(),ids.end());
  std::sort(ordered.begin(),ordered.end());return ordered;
}
inline Json progressJson(const MessageRefs& messages){
  Json result=Json::array();for(const auto& message:messages)result.push_back(*message);return result;
}
inline GadgetIDs ids(const Json& value){
  GadgetIDs result;if(value.is_array())for(const auto& id:value)result.insert(id.get<std::string>());
  return result;
}
inline RenderItem renderItem(const Json& value){
  RenderItem item(std::make_shared<Json>(value));
  if(field(value,"type")=="grouped_tool_use"){
    for(const auto& member:value.at("messages"))item.members.push_back(std::make_shared<Json>(member));
    for(const auto& result:value.value("results",Json::array()))item.results.push_back(std::make_shared<Json>(result));
    item.displayMessage=std::make_shared<Json>(value.at("displayMessage"));
  }else if(field(value,"type")=="collapsed_read_search"){
    item.collapsedItems=std::make_shared<RenderItems>();
    for(const auto& child:value.at("messages"))item.collapsedItems->push_back(renderItem(child));
    item.displayItem=std::make_shared<RenderItem>(renderItem(value.at("displayMessage")));
  }
  return item;
}
inline MessageOptions options(const Json& spec){
  MessageOptions out;const auto value=spec.value("options",Json::object());
  out.addMargin=value.value("addMargin",true);out.verbose=value.value("verbose",false);
  out.transcriptMode=value.value("isTranscriptMode",false);
  out.shouldAnimate=value.value("shouldAnimate",false);out.shouldShowDot=value.value("shouldShowDot",true);
  out.isStatic=value.value("isStatic",false);out.isUserContinuation=value.value("isUserContinuation",false);
  out.isActiveCollapsedGroup=value.value("isActiveCollapsedGroup",false);
  out.columns=value.value("columns",80);
  if(field(value,"containerWidth").is_number())out.containerWidth=value.at("containerWidth").get<int>();
  const auto optional=[&](const char* key){return value.contains(key)?value.at(key):Json(Json::value_t::discarded);};
  out.lastThinkingBlockId=optional("lastThinkingBlockId");
  out.latestBashOutputUUID=optional("latestBashOutputUUID");
  out.width=optional("width");out.style=optional("style");
  out.inProgressGadgetUseIDs=ids(field(value,"inProgressToolUseIDs"));
  for(const auto& event:value.value("progressMessagesForMessage",Json::array()))
    out.progressMessages.push_back(std::make_shared<Json>(event));
  return out;
}
inline std::shared_ptr<MessageLookups> lookups(const Json& spec){
  auto out=std::make_shared<MessageLookups>();const auto value=spec.value("lookups",Json::object());
  out->resolvedGadgetUseIDs=ids(field(value,"resolved"));out->erroredGadgetUseIDs=ids(field(value,"errored"));
  if(field(value,"siblings").is_object())for(const auto& [id,siblings]:value.at("siblings").items()){
    auto set=std::make_shared<batbox::runtime::OrderedStringSet>();
    for(const auto& sibling:siblings)set->insert(sibling.get<std::string>());
    out->siblingGadgetUseIDs[id]=std::move(set);
  }
  if(field(value,"progress").is_object())for(const auto& [id,events]:value.at("progress").items())
    for(const auto& event:events)out->progressMessagesByGadgetUseID[id].push_back(std::make_shared<Json>(event));
  for(const auto* key:{"hooks","resolvedHooks"})if(field(value,key).is_object()){
    auto& target=std::string(key)=="hooks"?out->inProgressHookCounts:out->resolvedHookCounts;
    for(const auto& [id,events]:value.at(key).items())for(const auto& [name,count]:events.items())
      target[id][name]=count.get<int>();
  }
  return out;
}
inline RowRequest row(const Json& spec){
  const auto opts=spec.value("options",Json::object());
  return {renderItem(spec.at("message")),options(spec),lookups(spec),
    ids(field(opts,"streamingToolUseIDs")),opts.value("screen",std::string("prompt")),
    opts.value("canAnimate",false),opts.value("isLoading",false),opts.value("hasContentAfter",false)};
}
Json messageTree(const MessagePlan& plan);
Json rowTree(const RowPlan& plan);
} // namespace message_view_fixture
