#include "application/model/LocalModelService.hpp"
#include "compaction/FlightPolicy.hpp"
#include "diagnostics/LogDates.hpp"
#include "query/AsyncSetup.hpp"
#include "runtime/Platform.hpp"
#include "services/tokenEstimation.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace batbox::application::model {
namespace {
const Json& property(const Json& source,const char* name){
  return value::property(source,name);
}
bool typeIs(const Json& source,const char* name){
  return value::stringEquals(property(source,"type"),name);
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

std::optional<Json> LocalModelService::clearOldGadgetResults(
    const Json& messages,const std::string& querySource,
    const execution::Context& context){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Microcompact requires owner loop");
  if(!messages.is_array())throw std::invalid_argument("Messages must be an array");
  if(imports_.clearCompactWarningSuppression)
    imports_.clearCompactWarningSuppression();
  const auto& policy=compaction::flightPolicy(querySource);
  if(policy.budgetMicrocompact){
    // Dispatch-lane pruning: fold older tool results deterministically when
    // the thread exceeds the window share, so no summarizer turn runs on the
    // dispatched model. Nothing else about the thread is rewritten.
    if(value::truthy(value::property(config_.environment,"BATBOX_DISABLE_COMPACT")))
      return std::nullopt;
    const auto& model=value::property(value::property(context.data,"options"),
      "mainLoopModel");
    if(!model.is_string())return std::nullopt;
    const auto profile=findProfile(model.get<std::string>());
    if(!profile||profile->contextTokens<=0)return std::nullopt;
    const double threshold=policy.pruneFraction*profile->contextTokens;
    if(estimateMessageTokens(messages)<=threshold)return std::nullopt;
    const auto pruned=compaction::pruneOldToolResults(messages,
      policy.keepRecentToolResults,gadgetResultTokens);
    if(!pruned.changed)return std::nullopt;
    imports_.debug("debug","[DISPATCH-MC] window "+
      value::numberToString(profile->contextTokens)+", cleared "+
      std::to_string(pruned.cleared)+" tool results (~"+
      value::numberToString(pruned.tokensSaved)+" tokens), kept last "+
      std::to_string(policy.keepRecentToolResults));
    imports_.suppressCompactWarning();
    return pruned.messages;
  }
  if(!config_.timeBasedMicrocompact||querySource.empty()||
      !mainThread(querySource))return std::nullopt;
  const Json* lastAssistant=nullptr;
  for(auto it=messages.rbegin();it!=messages.rend();++it)
    if(typeIs(*it,"assistant")){lastAssistant=&*it;break;}
  if(!lastAssistant||!property(*lastAssistant,"timestamp").is_string())
    return std::nullopt;
  const double timestamp=diagnostics::parseLogISOString(
    property(*lastAssistant,"timestamp").get<std::string>());
  const double gapMinutes=(static_cast<double>(platform::nowMillis())-
    timestamp)/60000;
  if(!std::isfinite(gapMinutes)||gapMinutes<config_.timeBasedGapMinutes)
    return std::nullopt;
  const auto keepRecent=static_cast<std::size_t>(std::max(1,
    config_.timeBasedKeepRecent));
  const auto pruned=compaction::pruneOldToolResults(messages,keepRecent,
    gadgetResultTokens);
  if(!pruned.changed)return std::nullopt;
  imports_.debug("debug","[TIME-BASED MC] gap "+
    value::numberToString(value::round(gapMinutes))+"min > "+
    value::numberToString(config_.timeBasedGapMinutes)+"min, cleared "+
    std::to_string(pruned.cleared)+" tool results (~"+
    value::numberToString(pruned.tokensSaved)+" tokens), kept last "+
    std::to_string(keepRecent));
  imports_.suppressCompactWarning();
  return pruned.messages;
}

event_loop::Deferred<Json> LocalModelService::microcompactMessages(
    const Json& messages,const execution::Context& context,
    const std::string& querySource){
  auto cleared=clearOldGadgetResults(messages,querySource,context);
  event_loop::Deferred<Json> response(owner_);
  response.resolve(Json{{"messages",cleared?std::move(*cleared):messages}});
  return response;
}

event_loop::Deferred<Json> LocalModelService::microcompactForQuery(
    const Json& messages,const execution::Context& context,
    const std::string& querySource){
  auto cleared=clearOldGadgetResults(messages,querySource,context);
  event_loop::Deferred<Json> response(owner_);
  response.resolve(Json{{"messages",cleared?std::move(*cleared):query::unchangedMessages()}});
  return response;
}
} // namespace batbox::application::model
