#include "query/Engine.hpp"
#include "query/StopHooks.hpp"
#include "retry/Retry.hpp"
#include <cassert>
#include <future>
#include <iostream>

using namespace batbox;
template<class T>std::future<T> ready(T value){std::promise<T> p;p.set_value(std::move(value));return p.get_future();}
static Json makeUser(const Json& args){
  Json result={{"type","user"},{"message",{{"content",args.at("content")}}}};
  for(const auto& [key,value]:args.items())if(key!="content")result[key]=value;
  return result;
}
static Json assistant(Json content,int n){
  return Json{{"type","assistant"},{"uuid","assistant-"+std::to_string(n)},
    {"message",{{"id","message-"+std::to_string(n)},{"content",content}}}};
}
static model::NextMessage stream(std::vector<Json> messages){
  return [messages=std::move(messages),i=std::size_t{0}]() mutable
    ->std::optional<model::MessageRef>{
      if(i==messages.size())return std::nullopt;
      return std::make_shared<Json>(messages[i++]);
    };
}
int main(int argc,char** argv){
  std::string scenario="tool";bool jsonOutput=false;
  for(int i=1;i<argc;++i)if(std::string(argv[i])=="--json")jsonOutput=true;
    else scenario=argv[i];
  gadgets::Gadget gadget;gadget.name="Read";
  gadget.parseInput=[](const Json& input){return input;};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.call=[](const Json&,const gadgets::GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const gadgets::GadgetProgress&)>&){
    assert(!context.stopToken.stop_requested());
    gadgets::GadgetResult result;result.data=Json{{"text","read done"}};return ready(result);
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    return Json{{"type","tool_result"},{"tool_use_id",id},{"content",data.at("text")}};
  };
  execution::Services execution;
  execution.createUserMessage=makeUser;
  execution.preGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
    const execution::Context&,const std::function<void(const Json&)>&){};
  execution.resolvePermissionDecision=[](const Json&,const gadgets::Gadget&,const Json& input,
    const execution::Context&,const Json&,const std::string&){
    return Json{{"decision",{{"behavior","allow"}}},{"input",input}};
  };
  execution.postGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
    const execution::Context&,const std::function<void(const Json&)>&){};
  execution.processPreMappedGadgetResultBlock=[](const Json& block,const gadgets::Gadget&){return block;};
  execution.canUseGadget=[](const Json&){return ready(Json{{"behavior","allow"}});};
  execution.addToGadgetDuration=[](double){};
  execution.nowMillis=[]{return std::int64_t{1};};
  execution.markGadgetUseInProgress=[](const std::string&){};
  execution.markGadgetUseComplete=[](const std::string&){};

  query::EngineServices services;
  services.config.getFlightId=[]{return "session";};
  services.config.statsigFeatureGate=[](const std::string&){return false;};
  services.setup.feature=[](const std::string&){return false;};
  services.setup.uuid=[]{return "chain";};
  services.setup.getMessagesAfterCompactBoundary=[](const Json& messages){return messages;};
  services.setup.applyGadgetResultBudget=[](const Json& messages,const Json&,
    const std::function<void(const Json&)>&,const std::vector<std::string>&){return messages;};
  services.setup.microcompact=[](const Json& messages,const execution::Context&,const std::string&){
    return Json{{"messages",messages}};
  };
  services.setup.appendSystemContext=[](const Json& poetry,const Json&){return poetry;};
  services.setup.autocompact=[](const Json&,const execution::Context&,const Json&,
    const std::string&,const Json&,double){return Json{{"compactionResult",nullptr}};};
  services.setup.getAppState=[](const execution::Context&){
    return Json{{"toolPermissionContext",{{"mode","default"}}},
      {"mcp",{{"tools",Json::array()},{"clients",Json::array()}}}};
  };
  services.setup.getRuntimeMainLoopModel=[](const Json&){return "test-model";};
  services.setup.tokenCountWithEstimation=[](const Json&){return 0.0;};
  services.setup.calculateTokenWarningState=[](double,const Json&){
    return Json{{"isAtBlockingLimit",false}};
  };
  services.setup.executionServices=&execution;
  services.model.feature=services.setup.feature;
  services.model.prependUserContext=[](const Json& messages,const Json&){return messages;};
  services.model.createUserMessage=makeUser;
  services.model.getAppState=services.setup.getAppState;
  services.model.createAssistantAPIErrorMessage=[](const Json& args){
    return Json{{"type","assistant"},{"isApiErrorMessage",true},{"args",args}};
  };
  services.model.createSystemMessage=[](const std::string& text,const std::string& level){
    return Json{{"type","system"},{"text",text},{"level",level}};
  };
  services.model.renderModelName=[](const std::string& model){return model;};
  services.completion.feature=services.setup.feature;
  services.completion.isPoetryTooLongMessage=[](const Json&){return false;};
  services.completion.getFeatureValue=[](const std::string&,bool){return false;};
  services.completion.createUserMessage=makeUser;
  services.completion.executeStopFailureHooks=[](const Json&,const execution::Context&){};
  int stopHookCalls=0;
  query::StopHookServices stopServices;
  stopServices.feature=services.setup.feature;
  stopServices.createStashSafeParams=[](const Json& context){return context;};
  stopServices.saveStashSafeParams=[](const Json&){};
  stopServices.isBareMode=[]{return true;};
  stopServices.getAppState=services.setup.getAppState;
  stopServices.executeStopHooks=[&](const Json&,const execution::Context&,const query::HookResultSink& yield){
    ++stopHookCalls;
    if(scenario=="stop_hook"&&stopHookCalls==1)
      yield(Json{{"blockingError",{{"blockingError","blocked"}}}});
  };
  stopServices.getStopHookMessage=[](const Json& error){return value::property(error,"blockingError");};
  stopServices.createUserMessage=makeUser;
  stopServices.createAttachmentMessage=[](const Json& attachment){
    return Json{{"type","attachment"},{"attachment",attachment}};
  };
  stopServices.isPartner=[]{return false;};
  stopServices.createSystemMessage=[](const std::string& text,const std::string& level){
    return Json{{"type","system"},{"text",text},{"level",level}};
  };
  services.completion.handleStopHooks=[&](const Json& messages,const Json& assistants,
    const query::Params& params,const execution::Context& context,
    const std::optional<bool>& active,const query::Yield& yield){
    return query::handleStopHooks(messages,assistants,params,context,active,stopServices,yield);
  };
  services.gadgetTurn.feature=services.setup.feature;
  services.gadgetTurn.executionServices=&execution;
  services.gadgetTurn.createAttachmentMessage=[](const Json& attachment){
    return Json{{"type","attachment"},{"attachment",attachment}};
  };
  services.gadgetTurn.normalizeMessagesForAPI=[](const Json& messages,const gadgets::Gadgets&){return messages;};
  services.gadgetTurn.getCommandsByMaxPriority=[](const std::string&){return Json::array();};
  services.gadgetTurn.isSlashCommand=[](const Json&){return false;};
  services.gadgetTurn.getAttachmentMessages=[](const Json&,const execution::Context&,
    const Json&,const std::string&){return Json::array();};
  services.startRelevantMemoryPrefetch=[](const Json&,const execution::Context&){
    query::MemoryPrefetch prefetch;prefetch.isSettled=[]{return false;};return prefetch;
  };
  int sampling=0;
  services.executePostSamplingHooks=[&](const Json&,const query::Params&,
    const execution::Context&){++sampling;};

  query::Params params;params.querySource="sdk";params.messages=Json::array();
  if(scenario=="max_turns")params.maxTurns=1;
  if(scenario=="fallback")params.fallbackModel="fallback";
  params.gadgetUseContext.gadgets={&gadget};
  params.gadgetUseContext.data={{"options",{{"mainLoopModel","test-model"},
    {"agentDefinitions",{{"activeAgents",Json::object()},
      {"allowedAgentTypes",Json::array()}}}}}};
  const Json gadgetMessage=assistant(Json::array({Json{{"type","tool_use"},{"name","Read"},
    {"id","tool-1"},{"input",Json::object()}}}),1);
  const Json doneMessage=assistant(Json::array({Json{{"type","text"},{"text","done"}}}),2);
  int modelCalls=0;
  services.model.callModel=[&](const query::ModelRequest& request,const std::function<void()>&)
    ->model::NextMessage{
    ++modelCalls;
    if(scenario=="plain")return stream({doneMessage});
    if(scenario=="api_error")return stream({Json{{"type","assistant"},
      {"isApiErrorMessage",true},{"apiError","invalid_request"},
      {"message",{{"content",Json::array()}}}}});
    if(scenario=="max_output"){
      if(modelCalls==1)return stream({Json{{"type","assistant"},
        {"apiError","max_output_tokens"},{"isApiErrorMessage",true},
        {"message",{{"content",Json::array()}}}}});
      return stream({doneMessage});
    }
    if(scenario=="stop_hook")return stream({doneMessage});
    if(scenario=="fallback"){
      if(modelCalls==1)return [gadgetMessage,once=false]() mutable
        ->std::optional<model::MessageRef>{
          if(!once){once=true;return std::make_shared<Json>(gadgetMessage);}
          throw retry::FallbackTriggeredError("test-model","fallback");
        };
      assert(request.options.at("model")=="fallback");
      return stream({doneMessage});
    }
    assert(request.options.at("model")=="test-model");
    return stream({modelCalls==1?gadgetMessage:doneMessage});
  };
  Json emitted=Json::array();
  const Json terminal=query::query(params,services,[&](const Json& message){emitted.push_back(message);});
  if(scenario=="tool"){
    assert(terminal.at("reason")=="completed");
    assert(modelCalls==2&&sampling==2&&emitted.size()==5);
    assert(emitted[0]["type"]=="stream_request_start");
    assert(emitted[1]==gadgetMessage);
    assert(emitted[2]["type"]=="user");
    assert(emitted[2]["message"]["content"][0]["content"]=="read done");
    assert(emitted[3]["type"]=="stream_request_start"&&emitted[4]==doneMessage);
  }
  const Json output={{"terminal",terminal},{"emitted",emitted},
    {"modelCalls",modelCalls},{"sampling",sampling},{"stopHookCalls",stopHookCalls}};
  if(scenario=="tool"){
    int disposals=0;bool failDispose=false;
    services.startRelevantMemoryPrefetch=[&](const Json&,const execution::Context&){
      query::MemoryPrefetch prefetch;prefetch.isSettled=[]{return false;};
      prefetch.dispose=[&]{++disposals;if(failDispose)throw std::runtime_error("dispose");};
      return prefetch;
    };
    query::query(params,services,[](const Json&){});
    const auto originalBoundary=services.setup.getMessagesAfterCompactBoundary;
    services.setup.getMessagesAfterCompactBoundary=[](const Json&)->Json{
      throw std::runtime_error("body");
    };
    bool sawBody=false;
    try{query::query(params,services,[](const Json&){});}catch(const std::runtime_error& e){
      sawBody=std::string(e.what())=="body";
    }
    assert(sawBody);
    services.setup.getMessagesAfterCompactBoundary=originalBoundary;
    failDispose=true;
    bool sawDispose=false;
    try{query::query(params,services,[](const Json&){});}catch(const std::runtime_error& e){
      sawDispose=std::string(e.what())=="dispose";
    }
    assert(sawDispose);
    services.setup.getMessagesAfterCompactBoundary=[](const Json&)->Json{
      throw std::runtime_error("body");
    };
    bool sawBoth=false;
    try{query::query(params,services,[](const Json&){});}catch(const query::QuerySuppressedError& e){
      try{std::rethrow_exception(e.disposalError);}catch(const std::runtime_error& disposal){
        try{std::rethrow_exception(e.priorError);}catch(const std::runtime_error& prior){
          sawBoth=std::string(disposal.what())=="dispose"&&std::string(prior.what())=="body";
        }
      }
    }
    assert(sawBoth&&disposals==4);
  }
  if(jsonOutput)std::cout<<value::stringify(output)<<'\n';
  else std::cout<<"query model/tool/model cycle, request starts, actual tool dispatch and completion passed\n";
}
