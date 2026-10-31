#include "query/AsyncSetupInternal.hpp"
#include <cmath>

namespace batbox::query {
AsyncSetupIterator makeAsyncSetupIterator(event_loop::Loop& loop,Params params,
  State state,QueryConfig config,AsyncSetupServices services,
  std::optional<double> missionBudgetRemaining){
  if(!loop.isOwnerThread())
    throw std::logic_error("Async query preparation must start on the owner loop");
  return AsyncSetupIterator(std::make_shared<async_setup::Run>(loop,std::move(params),
    std::move(state),std::move(config),std::move(services),missionBudgetRemaining));
}
event_loop::Deferred<AsyncSetupStep> AsyncSetupIterator::next(){return run_->next();}
} // namespace batbox::query

namespace batbox::query::async_setup {
event_loop::Deferred<AsyncSetupStep> Run::next(){
  if(!loop.isOwnerThread())throw std::logic_error("Async setup next() requires loop owner");
  event_loop::Deferred<AsyncSetupStep> request(loop);
  requests.push_back(request);
  pump();
  return request;
}
void Run::emit(const Json& event,Stage after){
  stage=after;
  pausedAfterYield=true;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncSetupStep{false,event,{}});
  if(!requests.empty()){
    auto self=shared_from_this();
    loop.queueMicrotask([self]{self->pump();});
  }
}
void Run::finish(){
  stage=Stage::Done;
  auto prepared=std::make_shared<AsyncPreparedResult>();
  prepared->iteration=std::move(out);
  prepared->missionBudgetRemaining=remaining;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncSetupStep{true,{},std::move(prepared)});
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncSetupStep{true,{},{}});
  }
}
void Run::fail(std::exception_ptr error){
  failure=std::move(error);stage=Stage::Done;
  if(!requests.empty()){
    auto request=requests.front();requests.pop_front();request.reject(failure);
  }
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncSetupStep{true,{},{}});
  }
}
void Run::pump(){
  if(requests.empty())return;
  try{
    while(!requests.empty()){
      switch(stage){
      case Stage::Start:start();break;
      case Stage::AfterStart:startBudget();break;
      case Stage::AwaitBudget:case Stage::AwaitMicro:case Stage::AwaitCollapse:
      case Stage::AwaitAuto:return;
      case Stage::AfterBudget:afterBudget(out.messagesForQuery);break;
      case Stage::AfterSnip:startMicrocompact();break;
      case Stage::AfterMicro:startCollapses();break;
      case Stage::AfterCollapse:startAutocompact();break;
      case Stage::YieldPost:
        if(postIndex<postMessages.size())emit(postMessages[postIndex++],Stage::YieldPost);
        else{out.messagesForQuery=postMessages;stage=Stage::AfterPost;}
        break;
      case Stage::AfterPost:finishSetup();break;
      case Stage::AfterBlocking:finish();break;
      case Stage::Done:{
        auto request=requests.front();requests.pop_front();
        request.resolve(AsyncSetupStep{true,{},{}});
        break;
      }
      }
      if(pausedAfterYield){pausedAfterYield=false;return;}
    }
  }catch(...){fail(std::current_exception());}
}

void Run::start(){
  need(services.sync.feature,"feature");need(services.sync.uuid,"uuid");
  out.gadgetUseContext=state.gadgetUseContext;
  out.executionServices=services.sync.executionServices;
  if(services.sync.feature("EXPERIMENTAL_SKILL_SEARCH")){
    need(services.sync.startSkillDiscoveryPrefetch,"startSkillDiscoveryPrefetch");
    out.pendingSkillPrefetch=services.sync.startSkillDiscoveryPrefetch(
      state.messages,out.gadgetUseContext);
  }
  emit(Json{{"type","stream_request_start"}},Stage::AfterStart);
}

void Run::startBudget(){
  need(services.queryCheckpoint,"queryCheckpoint");
  services.queryCheckpoint("query_fn_entry");
  if(!value::truthy(field(out.gadgetUseContext.data,"agentId"))){
    need(services.headlessProfilerCheckpoint,"headlessProfilerCheckpoint");
    services.headlessProfilerCheckpoint("query_started");
  }
  const Json& prior=field(out.gadgetUseContext.data,"queryTracking");
  if(value::truthy(prior)){
    const Json& depth=field(prior,"depth");
    out.queryTracking={{"chainId",field(prior,"chainId")},
      {"depth",depth.is_number()?depth.get<double>()+1:1}};
  }else out.queryTracking={{"chainId",services.sync.uuid()},{"depth",0}};
  out.gadgetUseContext.data["queryTracking"]=out.queryTracking;
  need(services.sync.getMessagesAfterCompactBoundary,"getMessagesAfterCompactBoundary");
  out.messagesForQuery=services.sync.getMessagesAfterCompactBoundary(state.messages);
  out.tracking=state.autoCompactTracking;
  const bool persistReplacements=params.querySource.starts_with("agent:")||
    params.querySource.starts_with("repl_main_thread");
  std::vector<std::string> unlimited;
  for(const auto* gadget:out.gadgetUseContext.gadgets)
    if(gadget&&!std::isfinite(gadget->maxResultSizeChars))unlimited.push_back(gadget->name);
  std::function<void(const Json&)> persist;
  if(persistReplacements){
    need(services.sync.recordContentReplacement,"recordContentReplacement");
    persist=[self=shared_from_this(),batId=field(out.gadgetUseContext.data,"agentId")]
      (const Json& records){self->services.sync.recordContentReplacement(records,batId);};
  }
  need(services.applyGadgetResultBudget,"applyToolResultBudget");
  auto pending=services.applyGadgetResultBudget(out.messagesForQuery,
    out.gadgetUseContext.contentReplacementState,persist,unlimited);
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async budget import belongs to another event loop");
  stage=Stage::AwaitBudget;
  auto self=shared_from_this();
  pending.then([self](const Json* value,const std::exception_ptr& error){
    if(error){self->fail(error);return;}
    try{self->out.messagesForQuery=*value;self->stage=Stage::AfterBudget;self->pump();}
    catch(...){self->fail(std::current_exception());}
  });
}

void Run::afterBudget(const Json&){
  snipEnabled=services.sync.feature("HISTORY_SNIP");
  if(snipEnabled){
    services.queryCheckpoint("query_snip_start");
    need(services.sync.snipCompactIfNeeded,"snipCompactIfNeeded");
    const Json snipped=services.sync.snipCompactIfNeeded(out.messagesForQuery);
    out.messagesForQuery=field(snipped,"messages");
    const Json& freed=field(snipped,"tokensFreed");
    if(freed.is_number())out.snipTokensFreed=freed.get<double>();
    if(!field(snipped,"boundaryMessage").is_null()){
      emit(field(snipped,"boundaryMessage"),Stage::AfterSnip);
      return;
    }
  }
  stage=Stage::AfterSnip;
}

void Run::startMicrocompact(){
  if(snipEnabled)services.queryCheckpoint("query_snip_end");
  services.queryCheckpoint("query_microcompact_start");
  need(services.microcompact,"microcompact");
  auto pending=services.microcompact(out.messagesForQuery,
    out.gadgetUseContext,params.querySource);
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async microcompact import belongs to another event loop");
  stage=Stage::AwaitMicro;
  auto self=shared_from_this();
  pending.then([self](const Json* value,const std::exception_ptr& error){
    if(error){self->fail(error);return;}
    try{self->afterMicrocompact(*value);self->pump();}
    catch(...){self->fail(std::current_exception());}
  });
}

void Run::afterMicrocompact(const Json& micro){
  out.messagesForQuery=field(micro,"messages");
  if(services.sync.feature("CACHED_MICROCOMPACT"))
    out.pendingStashEdits=field(field(micro,"compactionInfo"),"pendingCacheEdits");
  services.queryCheckpoint("query_microcompact_end");
  stage=Stage::AfterMicro;
}

void Run::startCollapses(){
  if(services.sync.feature("CONTEXT_COLLAPSE") && services.applyCollapsesIfNeeded){
    auto pending=services.applyCollapsesIfNeeded(out.messagesForQuery,
      out.gadgetUseContext,params.querySource);
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Async collapse import belongs to another event loop");
    stage=Stage::AwaitCollapse;
    auto self=shared_from_this();
    pending.then([self](const Json* value,const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{self->afterCollapses(*value);self->pump();}
      catch(...){self->fail(std::current_exception());}
    });
  }else stage=Stage::AfterCollapse;
}
void Run::afterCollapses(const Json& collapsed){
  out.messagesForQuery=field(collapsed,"messages");
  stage=Stage::AfterCollapse;
}

void Run::startAutocompact(){
  need(services.sync.appendSystemContext,"appendSystemContext");
  out.fullSystemPoetry=services.sync.appendSystemContext(params.systemPoetry,params.systemContext);
  services.queryCheckpoint("query_autocompact_start");
  event_loop::Deferred<Json> pending=[&]{
    if(params.indigenousBatContext){
      need(services.sync.makeIndigenousStashSafeParams,"makeNativeCacheSafeParams");
      need(services.autocompactIndigenous,"autocompactNative");
      auto snapshot=services.sync.makeIndigenousStashSafeParams(params.indigenousBatContext,
        params.systemPoetry,params.userContext,params.systemContext,
        out.gadgetUseContext,out.messagesForQuery);
      return services.autocompactIndigenous(out.messagesForQuery,out.gadgetUseContext,snapshot,
        params.querySource,out.tracking,out.snipTokensFreed);
    }
    need(services.autocompact,"autocompact");
    const Json stashSafe={{"systemPrompt",params.systemPoetry},{"userContext",params.userContext},
      {"systemContext",params.systemContext},{"toolUseContext",out.gadgetUseContext.data},
      {"forkContextMessages",out.messagesForQuery}};
    return services.autocompact(out.messagesForQuery,out.gadgetUseContext,stashSafe,
      params.querySource,out.tracking,out.snipTokensFreed);
  }();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async autocompact import belongs to another event loop");
  stage=Stage::AwaitAuto;
  auto self=shared_from_this();
  pending.then([self](const Json* value,const std::exception_ptr& error){
    if(error){self->fail(error);return;}
    try{self->services.queryCheckpoint("query_autocompact_end");
      self->afterAutocompact(*value);self->pump();}
    catch(...){self->fail(std::current_exception());}
  });
}
} // namespace batbox::query::async_setup
