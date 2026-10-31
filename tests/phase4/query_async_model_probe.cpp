#include "query/AsyncModelPass.hpp"
#include "retry/Retry.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace batbox;
namespace {
const Json gadgetBlock={{"type","tool_use"},{"id","t1"},{"name","Read"},
  {"input",{{"path","a"}}}};
Json assistant(const std::string& id,const Json& content){
  return {{"type","assistant"},{"uuid","u-"+id},{"message",{{"id",id},
    {"content",content},{"usage",{{"input_tokens",26}}},{"stop_reason",nullptr}}}};
}
Json resultValue(const query::AsyncModelStep& step,const query::PreparedIteration& prepared){
  if(step.value->terminal)return step.value->terminal->at("reason");
  return {{"assistantMessages",step.value->assistantMessages},
    {"toolUseBlocks",step.value->gadgetUseBlocks},
    {"needsFollowUp",step.value->needsFollowUp},{"currentModel",prepared.currentModel}};
}
}
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"plain";
  event_loop::Loop loop;
  Json trace=Json::array();
  query::Params params;params.querySource="sdk";params.messages=Json::array();
  if(scenario=="fallback")params.fallbackModel="fallback";
  query::State state;query::QueryConfig config;
  auto prepared=std::make_shared<query::PreparedIteration>();
  prepared->messagesForQuery=Json::array();prepared->fullSystemPoetry=Json::array();
  prepared->currentModel="main";prepared->queryTracking={{"chainId","chain"},{"depth",0}};
  prepared->appState={{"mcp",{{"clients",Json::array()},{"tools",Json::array()}}}};
  prepared->gadgetUseContext.data={{"options",{{"thinkingConfig",nullptr},
    {"agentDefinitions",{{"activeAgents",Json::object()},
      {"allowedAgentTypes",Json::array()}}}}}};
  auto first=std::make_shared<Json>(assistant("first",
    scenario=="fallback"||scenario=="streaming_fallback"||scenario=="error"||
    scenario=="clone_added"||scenario=="clone_overwrite"||scenario=="clone_fallback"?
    Json::array({gadgetBlock}):Json::array({Json{{"type","text"},{"text","hello"}}})));
  auto second=std::make_shared<Json>(assistant("second",Json::array({
    Json{{"type","text"},{"text","fallback answer"}}})));
  gadgets::Gadget gadget;gadget.name="Read";
  gadget.backfillObservableInput=[scenario](Json& input){
    if(scenario=="clone_added"||scenario=="clone_fallback")input["derived"]=7;
    if(scenario=="clone_overwrite")input["path"]="expanded";
  };
  prepared->gadgetUseContext.gadgets={&gadget};
  if(scenario=="cached")prepared->pendingStashEdits={{"trigger","cache"},
    {"baselineCacheDeletedTokens",2},{"deletedToolIds",Json::array({"t1"})}};
  query::AsyncModelServices services;
  services.queryCheckpoint=[&](const std::string& name){trace.push_back(Json::array({"checkpoint",name}));};
  services.logError=[&](const std::exception& error){trace.push_back(Json::array({"logError",error.what()}));};
  services.logInternalError=[&](const std::exception& error){
    trace.push_back(Json::array({"logInternalError",error.what()}));};
  services.sync.feature=[&](const std::string& name){return name=="CACHED_MICROCOMPACT"&&scenario=="cached";};
  services.sync.prependUserContext=[](const Json& messages,const Json&){return messages;};
  services.sync.getAppState=[](const execution::Context&){
    return Json{{"toolPermissionContext",{{"mode","default"}}}};};
  services.sync.createUserMessage=[](const Json& args){return Json{{"type","user"},{"args",args}};};
  services.sync.createAssistantAPIErrorMessage=[](const Json& args){
    return Json{{"type","assistant"},{"apiError","error"},{"args",args}};};
  services.sync.createSystemMessage=[](const std::string& text,const std::string& level){
    return Json{{"type","system"},{"text",text},{"level",level}};};
  services.sync.renderModelName=[](const std::string& name){return name;};
  services.sync.stripSignatureBlocks=[](const Json& messages){return messages;};
  services.sync.createMicrocompactBoundaryMessage=[](const Json& trigger,int estimated,
      double deleted,const Json& ids,const Json& extra){
    return Json{{"type","boundary"},{"trigger",trigger},{"estimated",estimated},
      {"deleted",deleted},{"ids",ids},{"extra",extra}};};
  services.sync.normalizeMessagesForAPI=[](const Json& messages,const gadgets::Gadgets&){return messages;};
  services.sync.isImageSizeOrResizeError=[](const std::exception& error){
    return std::string(error.what())=="image too large";};
  services.callModel=[&](const query::ModelRequest& request,
      const std::function<void()>& fallback)->query::AsyncModelCursor{
    const std::string model=request.options.at("model").get<std::string>();
    trace.push_back(Json::array({"call",model}));
    auto index=std::make_shared<int>(0);
    return query::AsyncModelCursor{[&,model,fallback,index]{
      event_loop::Deferred<std::optional<model::MessageRef>> pending(loop);
      const int n=(*index)++;
      if(scenario=="fallback"){
        if(model=="main"){
          if(n==0)pending.resolve(first);
          else pending.reject(std::make_exception_ptr(retry::FallbackTriggeredError("main","fallback")));
        }else if(n==0)pending.resolve(second);else pending.resolve(std::nullopt);
        return pending;
      }
      if(scenario=="streaming_fallback"||scenario=="clone_fallback"){
        if(n==0)pending.resolve(first);
        else if(n==1){fallback();pending.resolve(second);}
        else pending.resolve(std::nullopt);
        return pending;
      }
      if(scenario=="error"){
        if(n==0)pending.resolve(first);
        else pending.reject(std::make_exception_ptr(std::runtime_error("broken stream")));
        return pending;
      }
      if(scenario=="image_error"){
        pending.reject(std::make_exception_ptr(std::runtime_error("image too large")));
        return pending;
      }
      if(scenario=="withheld"){
        if(n==0){auto error=std::make_shared<Json>(*first);
          (*error)["apiError"]="max_output_tokens";
          (*error)["message"]["content"]=Json::array();
          pending.resolve(error);
        }else pending.resolve(std::nullopt);
        return pending;
      }
      if(n==0)pending.resolve(first);
      else{
        if(n==1&&(scenario=="late_mutation"||scenario=="clone_added"||
            scenario=="clone_overwrite"||scenario=="cached")){
          (*first)["message"]["usage"]["input_tokens"]=42;
          (*first)["message"]["stop_reason"]="end_turn";
          if(scenario=="cached")(*first)["message"]["usage"]["cache_deleted_input_tokens"]=7;
        }
        pending.resolve(std::nullopt);
      }
      return pending;
    }};
  };
  auto iterator=query::makeAsyncModelPassIterator(loop,params,state,prepared,config,
    std::nullopt,services);
  model::MessageRef firstYield;
  auto record=[&](const query::AsyncModelStep& step){
    if(step.done){if(step.value)trace.push_back(Json::array({"done",resultValue(step,*prepared)}));
      else trace.push_back(Json::array({"done",nullptr}));}
    else{
      if(!firstYield)firstYield=step.event;
      trace.push_back(Json::array({"yield",*step.event}));
      if(step.event->value("type","")=="tombstone"){
        trace.push_back(Json::array({"tombstoneIdentity",
          step.tombstoneTarget==first,step.tombstoneTarget==firstYield}));
      }else if(step.tombstoneTarget){
        throw std::logic_error("Non-tombstone event retained a target");
      }
    }
  };
  auto await=[&](event_loop::Deferred<query::AsyncModelStep> pending){
    std::optional<query::AsyncModelStep> step;std::exception_ptr error;
    pending.then([&](const query::AsyncModelStep* value,const std::exception_ptr& rejected){
      error=rejected;if(value)step=*value;});
    loop.runReady();if(error)std::rethrow_exception(error);
    if(!step)throw std::logic_error("Async model event did not settle");
    return *step;
  };
  auto firstRequest=iterator.next();
  std::optional<event_loop::Deferred<query::AsyncModelStep>> queued;
  if(scenario=="queued")queued=iterator.next();
  if(queued){
    std::exception_ptr error;
    auto observe=[&](const query::AsyncModelStep* value,const std::exception_ptr& rejected){
      if(rejected)error=rejected;else record(*value);};
    firstRequest.then(observe);queued->then(observe);
    loop.runReady();if(error)std::rethrow_exception(error);
  }else{
    auto step=await(firstRequest);record(step);
    while(!step.done){step=await(iterator.next());record(step);}
  }
  std::cout<<Json{{"trace",trace}}.dump()<<'\n';
}
