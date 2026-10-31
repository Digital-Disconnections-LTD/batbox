#include "application_model/LocalModelService.hpp"
#include "diagnostics/LogDates.hpp"
#include "runtime/Platform.hpp"
#include "services/tokenEstimation.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace batbox::application_model {
namespace {
const Json& property(const Json& source,const char* name){
  return value::property(source,name);
}
bool typeIs(const Json& source,const char* name){
  return property(source,"type")==name;
}
bool compactable(const Json& block){
  if(!typeIs(block,"tool_use"))return false;
  const auto& name=property(block,"name");
  if(!name.is_string())return false;
  static const std::set<std::string> names={
    "Read","Bash","PowerShell","Grep","Glob","WebSearch",
    "WebFetch","Edit","Write"};
  return names.contains(name.get<std::string>());
}
std::vector<std::string> collectCompactableGadgetIds(const Json& messages){
  std::vector<std::string> ids;
  for(const auto& message:messages){
    const auto& content=property(property(message,"message"),"content");
    if(!typeIs(message,"assistant")||!content.is_array())continue;
    for(const auto& block:content)
      if(compactable(block)&&property(block,"id").is_string())
        ids.push_back(property(block,"id").get<std::string>());
  }
  return ids;
}
double gadgetResultTokens(const Json& block){
  const auto& content=property(block,"content");
  if(!value::truthy(content))return 0;
  if(content.is_string())
    return roughTokenCountEstimation(content.get<std::string>());
  if(!content.is_array())return 0;
  double total=0;
  for(const auto& item:content){
    if(typeIs(item,"text")&&property(item,"text").is_string())
      total+=roughTokenCountEstimation(property(item,"text").get<std::string>());
    else if(typeIs(item,"image")||typeIs(item,"document"))total+=2000;
  }
  return total;
}
bool mainThread(const std::string& source){
  return source.rfind("repl_main_thread",0)==0;
}
}

double estimateMessageTokens(const Json& messages){
  double total=0;
  for(const auto& message:messages){
    if(!typeIs(message,"user")&&!typeIs(message,"assistant"))continue;
    const auto& content=property(property(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content){
      if(typeIs(block,"text"))
        total+=roughTokenCountEstimation(property(block,"text").get<std::string>());
      else if(typeIs(block,"tool_result"))total+=gadgetResultTokens(block);
      else if(typeIs(block,"image")||typeIs(block,"document"))total+=2000;
      else if(typeIs(block,"thinking"))
        total+=roughTokenCountEstimation(property(block,"thinking").get<std::string>());
      else if(typeIs(block,"redacted_thinking"))
        total+=roughTokenCountEstimation(property(block,"data").get<std::string>());
      else if(typeIs(block,"tool_use")){
        auto input=property(block,"input");
        if(input.is_null())input=Json::object();
        total+=roughTokenCountEstimation(
          property(block,"name").get<std::string>()+value::stringify(input));
      }else total+=roughTokenCountEstimation(value::stringify(block));
    }
  }
  return std::ceil(total*4/3);
}

event_loop::Deferred<Json> LocalModelService::microcompactMessages(
    const Json& messages,const execution::Context&,
    const std::string& querySource){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Microcompact requires owner loop");
  if(!messages.is_array())throw std::invalid_argument("Messages must be an array");
  event_loop::Deferred<Json> response(owner_);
  if(imports_.clearCompactWarningSuppression)
    imports_.clearCompactWarningSuppression();
  if(!config_.timeBasedMicrocompact||querySource.empty()||
      !mainThread(querySource)){
    response.resolve(Json{{"messages",messages}});return response;
  }
  const Json* lastAssistant=nullptr;
  for(auto it=messages.rbegin();it!=messages.rend();++it)
    if(typeIs(*it,"assistant")){lastAssistant=&*it;break;}
  if(!lastAssistant||!property(*lastAssistant,"timestamp").is_string()){
    response.resolve(Json{{"messages",messages}});return response;
  }
  const double timestamp=diagnostics::parseLogISOString(
    property(*lastAssistant,"timestamp").get<std::string>());
  const double gapMinutes=(static_cast<double>(platform::nowMillis())-
    timestamp)/60000;
  if(!std::isfinite(gapMinutes)||gapMinutes<config_.timeBasedGapMinutes){
    response.resolve(Json{{"messages",messages}});return response;
  }
  const auto compactableIds=collectCompactableGadgetIds(messages);
  const auto keepRecent=static_cast<std::size_t>(std::max(1,
    config_.timeBasedKeepRecent));
  std::set<std::string> keepSet;
  const auto firstKeep=compactableIds.size()>keepRecent
    ?compactableIds.size()-keepRecent:0;
  for(std::size_t i=firstKeep;i<compactableIds.size();++i)
    keepSet.insert(compactableIds[i]);
  std::set<std::string> clearSet;
  for(const auto& id:compactableIds)
    if(!keepSet.contains(id))clearSet.insert(id);
  if(clearSet.empty()){
    response.resolve(Json{{"messages",messages}});return response;
  }
  Json result=messages;
  double tokensSaved=0;
  for(auto& message:result){
    if(!typeIs(message,"user"))continue;
    auto& nested=message["message"];
    auto& content=nested["content"];
    if(!content.is_array())continue;
    for(auto& block:content){
      const auto& gadgetUseId=property(block,"tool_use_id");
      if(typeIs(block,"tool_result")&&gadgetUseId.is_string()&&
          clearSet.contains(gadgetUseId.get<std::string>())&&
          property(block,"content")!="[Old tool result content cleared]"){
        tokensSaved+=gadgetResultTokens(block);
        block["content"]="[Old tool result content cleared]";
      }
    }
  }
  if(tokensSaved==0){
    response.resolve(Json{{"messages",messages}});return response;
  }
  imports_.debug("debug","[TIME-BASED MC] gap "+
    value::numberToString(value::round(gapMinutes))+"min > "+
    value::numberToString(config_.timeBasedGapMinutes)+"min, cleared "+
    std::to_string(clearSet.size())+" tool results (~"+
    value::numberToString(tokensSaved)+" tokens), kept last "+
    std::to_string(keepSet.size()));
  imports_.suppressCompactWarning();
  if(config_.features.contains("PROMPT_CACHE_BREAK_DETECTION"))
    imports_.notifyStashDeletion(querySource);
  response.resolve(Json{{"messages",std::move(result)}});
  return response;
}
} // namespace batbox::application_model
