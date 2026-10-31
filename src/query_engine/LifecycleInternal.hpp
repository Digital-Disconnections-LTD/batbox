#pragma once
#include "query_engine/Conversation.hpp"
#include "query_engine/LifecycleServices.hpp"
#include "services/api/batbox.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::query_engine::lifecycle {
inline Json undefined(){return Json(Json::value_t::discarded);}
inline const Json& prop(const Json& value,std::string_view key){
  static const Json missing=undefined();
  if(!value.is_object())return missing;
  const auto at=value.find(std::string(key));return at==value.end()?missing:*at;
}
inline const Json& deref(const JsonRef& value){static const Json missing=undefined();return value?*value:missing;}
inline bool truth(const Json& value){return value::truthy(value);}
inline bool nullish(const Json& value){return value.is_null()||value.is_discarded();}
inline std::string string(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_discarded())return "undefined";
  return value.dump();
}
inline void put(Json& object,const std::string& key,const Json& value){
  if(!value.is_discarded())object[key]=value;
}
inline Json refArray(const MessagesRef& messages){
  Json values=Json::array();for(const auto& m:*messages)values.push_back(*m);return values;
}
inline bool hasType(const Json& message,const std::string& type){return prop(message,"type")==type;}
inline bool compact(const Json& message){return hasType(message,"system")&&prop(message,"subtype")=="compact_boundary";}
struct Run:std::enable_shared_from_this<Run> {
  enum class Stage {Start,PrepareContext,OrphanNext,UserInput,InputAfterRecord,
    InputAfterFlush,InitResume,LocalMessages,LocalPersist,LocalResult,QueryStart,
    QueryNext,QueryBeforeRecord,QueryAfterRecord,Ack,Dispatch,NormalizedNext,
    SystemAfterCompact,PostSwitch,RetryCheck,FinishQuery,FinalResult,Done};
  event_loop::Loop& loop;
  std::shared_ptr<ConversationState> state;
  std::shared_ptr<LifecycleServices> services;
  Configuration cfg;
  Json poetry;
  SubmitOptions options;
  Stage stage=Stage::Start;
  bool waiting=false,yielded=false,inBody=false,queryActive=false,orphanActive=false;
  std::optional<event_loop::Deferred<query::AsyncEngineStep>> request;
  std::optional<event_loop::Deferred<query::AsyncEngineStep>> closing;
  bool persistFlight=false;
  double startTime=0,turnCount=1,initialStructuredOutputCalls=0;
  AppStateRef initialAppState;
  std::string initialModel,mainLoopModel;
  JsonRef initialThinking;
  SystemPoetryParts parts;
  Json systemPoetry=Json::array(),userContext=Json::object(),memoryPoetry;
  ProcessContextRef context;
  CanUseGadget wrappedCanUseGadget;
  UserInputResult input;
  MessagesRef messages;
  Messages messagesToAck;
  std::size_t ackIndex=0,localIndex=0;
  int localPart=0;
  bool hasAcknowledgedInitialMessages=false;
  Json skills,enabledPlugins;
  MessageCursor orphanCursor,queryCursor;
  SdkCursor normalized;
  MessageRef message,errorLogWatermark,result;
  Json currentMessageUsage=EMPTY_USAGE,lastStopReason=nullptr,structuredOutput=undefined();
  std::string resultType,lastContentType;
  bool terminalAfterEmit=false;
  Run(event_loop::Loop& owner,std::shared_ptr<ConversationState> shared,
    Json value,SubmitOptions submit,std::shared_ptr<LifecycleServices> imports):
    loop(owner),state(std::move(shared)),services(std::move(imports)),
    poetry(std::move(value)),options(std::move(submit)){}
  event_loop::Deferred<query::AsyncEngineStep> next();
  event_loop::Deferred<query::AsyncEngineStep> close(Json value,std::exception_ptr error={});
  void pump();
  void emit(Json value,Stage after);
  void finish();
  void fail(std::exception_ptr error);
  template<class T,class F>void await(event_loop::Deferred<T> pending,F continuation){
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Conversation import crossed owner event loops");
    waiting=true;
    auto self=shared_from_this();
    pending.then([self,continuation=std::move(continuation)](const T* value,
        const std::exception_ptr& error) mutable {
      self->waiting=false;
      if(error){self->fail(error);return;}
      try{continuation(*value);self->pump();}catch(...){self->fail(std::current_exception());}
    });
  }
  void start();void afterPoetry();void prepareContext();void pullOrphan();
  ProcessContextRef makeContext(bool initial);
  void startInput();void afterInput(const UserInputResult& value);
  void afterInputRecord();void afterInputFlush();void loadSkills();
  void yieldInit();void afterInit();void localMessages();void localPersist();void localResult();
  void startQuery();void pullQuery();void beforeRecord();void record();void acknowledge();
  void dispatch();void normalizedNext();void attachment();void systemMessage();
  void systemAfterCompact();void postSwitch();void retryCheck();void finishQuery();void finalResult();
  void flushThen(std::function<void()> next);
  bool eagerFlush()const;
  Json resultBase(const std::string& subtype,bool isError,double turns);
  void limitResult(const std::string& subtype,double turns,std::string error);
  bool commandTag(const std::string& text)const;
};
}
