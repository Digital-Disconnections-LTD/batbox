#pragma once
#include "query/AsyncGadgetTurn.hpp"
#include <deque>

namespace batbox::query::async_gadget_turn {
struct Run:std::enable_shared_from_this<Run> {
  enum class Stage {Start,GadgetAwait,AfterGadgetYield,AfterGadgets,AbortCleanupAwait,
    AbortInterruption,AbortMaxTurns,AbortTerminal,AfterAbortMax,
    AfterSummary,AttachmentAwait,AfterAttachmentYield,
    MemoryStart,MemoryAwait,MemoryYield,AfterMemoryYield,
    SkillStart,SkillAwait,SkillYield,AfterSkillYield,
    Commands,Final,AfterMaxTurns,Done};
  event_loop::Loop& loop;
  Params params;
  State state;
  std::shared_ptr<PreparedIteration> prepared;
  std::shared_ptr<ModelPassResult> modelResult;
  QueryConfig config;
  MemoryPrefetch* memoryPrefetch;
  std::vector<std::string>& consumedCommandUuids;
  AsyncGadgetTurnServices services;
  Stage stage=Stage::Start;
  std::deque<event_loop::Deferred<AsyncDecisionStep>> requests;
  AsyncGadgetUpdateCursor gadgetCursor;
  AsyncMessageCursor attachmentCursor;
  std::optional<AsyncGadgetUpdate> currentGadgetUpdate;
  model::MessageRef currentAttachment;
  Json queuedCommands=Json::array();
  Json memoryAttachments=Json::array();
  Json skillAttachments=Json::array();
  std::size_t attachmentIndex=0;
  bool shouldPreventContinuation=false;
  bool pausedAfterYield=false;
  execution::Context updatedContext;
  std::shared_ptr<event_loop::Deferred<Json>> nextPendingSummary;
  std::exception_ptr failure;
  std::optional<event_loop::Deferred<event_loop::Unit>> closing;
  Run(event_loop::Loop& owner,Params p,State s,
    std::shared_ptr<PreparedIteration> prep,
    std::shared_ptr<ModelPassResult> result,QueryConfig c,
    MemoryPrefetch* memory,std::vector<std::string>& consumed,
    AsyncGadgetTurnServices sv)
    :loop(owner),params(std::move(p)),state(std::move(s)),
     prepared(std::move(prep)),modelResult(std::move(result)),config(std::move(c)),
     memoryPrefetch(memory),consumedCommandUuids(consumed),services(std::move(sv)),
     updatedContext(prepared->gadgetUseContext){}
  event_loop::Deferred<AsyncDecisionStep> next();
  event_loop::Deferred<event_loop::Unit> close();
  void pump();
  void emit(const model::MessageRef& event,Stage after);
  void finish(CompletionDecision value);
  void fail(std::exception_ptr error);
  void start();
  void pullGadgetUpdate();
  void afterGadgetYield();
  void afterGadgets();
  void startSummary();
  void startAbortCleanup();
  void emitAbortInterruption();
  void abortMaxTurns();
  void afterSummary();
  void startAttachments();
  void pullAttachment();
  void afterAttachmentYield();
  void startMemory();
  void yieldMemory();
  void afterMemoryYield();
  void startSkills();
  void yieldSkills();
  void afterSkillYield();
  void consumeCommands();
  void finalize();
};
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound async tool-turn import: ")+name);
}
inline const Json& field(const Json& object,const char* key){return value::property(object,key);}
inline Json appendMessages(const Json& a,const Json& b){
  Json out=a.is_array()?a:Json::array();
  if(b.is_array())for(const Json& item:b)out.push_back(item);
  return out;
}
} // namespace batbox::query::async_gadget_turn
