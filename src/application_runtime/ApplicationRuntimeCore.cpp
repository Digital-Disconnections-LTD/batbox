#include "application_runtime/ApplicationRuntime.hpp"
#include "interactive/TranscriptRows.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_runtime {
namespace {
void require(bool value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Application binding required: ")+name);
}
}
std::shared_ptr<ApplicationRuntime> ApplicationRuntime::create(Inputs inputs){
  auto owner=std::shared_ptr<ApplicationRuntime>(new ApplicationRuntime(std::move(inputs)));
  owner->initialize();return owner;
}
ApplicationRuntime::~ApplicationRuntime(){
  try{stop();}catch(...){if(inputs_.onDetachedError)
    inputs_.onDetachedError(std::current_exception());}
}
void ApplicationRuntime::initialize(){
  require(inputs_.owner.isOwnerThread(),"owner thread");
  require(static_cast<bool>(inputs_.store),"app state store");
  require(static_cast<bool>(inputs_.initialMessages),"initial messages");
  require(static_cast<bool>(inputs_.readFileState),"file state cache");
  require(static_cast<bool>(inputs_.engine),"query engine");
  require(static_cast<bool>(inputs_.nowMs),"clock");
  require(static_cast<bool>(inputs_.feature),"feature flags");
  require(static_cast<bool>(inputs_.onDetachedError),"detached error handler");
  require(static_cast<bool>(inputs_.makeQueryParams),"query parameters");
  require(static_cast<bool>(inputs_.bindSubmitProviders),"submit providers");
  require(static_cast<bool>(inputs_.bindRuntimeProviders),"runtime providers");
  require(static_cast<bool>(inputs_.makeSubmitRequest),"submit request");

  redrawSink_=inputs_.requestRedraw;
  auto weak=weak_from_this();
  queue_=std::make_unique<interactive::submit::CommandQueue>(inputs_.owner);
  auto messageServices=interactive::lifecycle::bindPresentationMessages(
    inputs_.presentation,[weak]{if(auto self=weak.lock())self->requestRedraw();});
  auto publishMessages=std::move(messageServices.publishMessages);
  messageServices.publishMessages=[weak,publish=std::move(publishMessages)](
      const interactive::TurnMessagesRef& values){
    publish(values);
    if(auto self=weak.lock())self->recordTranscriptChanges(values);
  };
  messages_=std::make_unique<interactive::lifecycle::MessageState>(
    inputs_.initialMessages,std::move(messageServices));
  inputs_.presentation.messages.assign(inputs_.initialMessages->begin(),
    inputs_.initialMessages->end());
  interactive::rebuildTranscriptRows(inputs_.presentation);
  ++inputs_.presentation.revision;

  turnState_=std::make_shared<interactive::QueryTurnState>();
  executionState_=std::make_shared<interactive::QueryExecutionState>();
  auto loadingServices=std::move(inputs_.loading);
  loadingServices.store=inputs_.store;
  loadingServices.readFileState=inputs_.readFileState;
  loadingServices.nowMs=inputs_.nowMs;
  loadingServices.requestRedraw=[weak]{if(auto self=weak.lock())
    self->requestRedraw();};
  loadingServices.onDetachedError=inputs_.onDetachedError;
  loading_=std::make_shared<interactive::lifecycle::LoadingState>(
    inputs_.owner,turnState_->guard,inputs_.presentation,*messages_,
    std::move(loadingServices));

  bindContext();
  bindQuery();
  bindSubmit();
  bindRuntime();
}
void ApplicationRuntime::start(){
  require(inputs_.owner.isOwnerThread(),"owner thread");
  if(started_)return;
  loading_->start();
  runtime_->start();
  started_=true;
}
void ApplicationRuntime::stop(){
  if(!started_)return;
  require(inputs_.owner.isOwnerThread(),"owner thread");
  started_=false;
  runtime_->stop();
}
event_loop::Deferred<event_loop::Unit> ApplicationRuntime::stopAndWait(){
  require(inputs_.owner.isOwnerThread(),"owner thread");
  if(stopWait_)return *stopWait_;
  event_loop::Deferred<event_loop::Unit> done(inputs_.owner);
  stopWait_=done;
  stopping_=true;
  const auto turns=activeTurns_;
  try{
    if(activeAbort_&&!activeAbort_->aborted())
      activeAbort_->abort("background");
    started_=false;
    auto diagnostics=runtime_->stopAndWait();
    auto remaining=std::make_shared<std::size_t>(turns.size()+1);
    auto weak=weak_from_this();
    const auto settle=[done,remaining,weak]{
      if(--*remaining!=0)return;
      if(auto self=weak.lock()){
        self->awaitTranscriptIdle().then([done](const event_loop::Unit*,
            const std::exception_ptr& error){
          if(error)done.reject(error);else done.resolve({});
        });
      }else done.resolve({});
    };
    for(const auto& turn:turns)
      turn.then([settle](const event_loop::Unit*,
          const std::exception_ptr&){settle();});
    diagnostics.then([settle](const event_loop::Unit*,
        const std::exception_ptr&){settle();});
  }catch(...){done.reject(std::current_exception());}
  return done;
}
event_loop::Deferred<event_loop::Unit>
ApplicationRuntime::awaitTranscriptIdle(){
  require(inputs_.owner.isOwnerThread(),"owner thread");
  event_loop::Deferred<event_loop::Unit> done(inputs_.owner);
  if(activeTranscriptRecords_.empty())done.resolve({});
  else transcriptIdleWaiters_.push_back(done);
  return done;
}
void ApplicationRuntime::setStreamSink(
    std::function<void(interactive::StreamInput)> sink){
  require(inputs_.owner.isOwnerThread(),"owner thread");
  streamSink_=std::move(sink);
}
void ApplicationRuntime::setModalSink(
    std::function<void(std::optional<interactive::UiRequest>)> sink){
  require(inputs_.owner.isOwnerThread(),"owner thread");
  modalSink_=std::move(sink);
}
void ApplicationRuntime::setRedrawSink(std::function<void()> sink){
  require(inputs_.owner.isOwnerThread(),"owner thread");
  redrawSink_=std::move(sink);
}
void ApplicationRuntime::requestRedraw()const{
  if(redrawSink_)redrawSink_();
}
interactive::submit::PoetrySubmitRequest ApplicationRuntime::poetryRequest(
    const std::string& text,const interactive::InputState& input)const{
  require(inputs_.owner.isOwnerThread(),"owner thread");
  return inputs_.makeSubmitRequest(text,input);
}
void ApplicationRuntime::publishMessageChanges(std::size_t beforeSize,
    const interactive::MessageRef& beforeLast){
  // Every list change made by applyStreamMessage (append, replace-last,
  // erase) moves the size or the last element; stream deltas move neither.
  const auto& after=inputs_.presentation.messages;
  if(beforeSize==after.size()&&(after.empty()?!beforeLast:after.back()==beforeLast))return;
  messages_->setMessages(std::make_shared<interactive::TurnMessages>(
    after.begin(),after.end()));
}
void ApplicationRuntime::recordTranscriptChanges(
    const interactive::TurnMessagesRef& messages){
  const auto& persistence=inputs_.transcriptPersistence;
  if(!persistence.flightId&&!persistence.record&&
      !persistence.lastChainParticipant)return;
  try{
    if(!persistence.flightId||!persistence.record||
        !persistence.lastChainParticipant)
      throw std::logic_error("Incomplete transcript persistence bindings");
    const auto flight=persistence.flightId();
    if(flight.empty())throw std::logic_error("Transcript session ID is empty");
    if(transcriptFlightId_!=flight){
      transcriptFlightId_=flight;
      lastRecordedLength_=0;
      firstRecordedUuid_.reset();
      lastRecordedParent_.reset();
      ++transcriptCallSeq_;
    }
    std::optional<std::string> first;
    if(!messages->empty()&&(*messages)[0]){
      const auto snapshot=runtime::snapshotMessage((*messages)[0]);
      const auto& uuid=value::property(snapshot,"uuid");
      if(uuid.is_string())first=uuid.get<std::string>();
    }
    const auto previousLength=lastRecordedLength_;
    const bool wasFirstRender=!firstRecordedUuid_;
    const bool sameHead=first&&firstRecordedUuid_&&
      first==firstRecordedUuid_;
    const bool incremental=sameHead&&previousLength<=messages->size();
    const bool sameHeadShrink=sameHead&&previousLength>messages->size();
    const auto start=incremental?previousLength:std::size_t{0};
    if(start==messages->size())return;
    std::vector<interactive::MessageRef> slice(messages->begin()+start,
      messages->end());
    auto parent=incremental?lastRecordedParent_:std::nullopt;
    const auto seq=++transcriptCallSeq_;
    auto pending=persistence.record(slice,parent,*messages);
    activeTranscriptRecords_.push_back(pending);
    auto weak=weak_from_this();
    pending.then([weak,seq,incremental,pending](const std::optional<std::string>* last,
        const std::exception_ptr& error){
      if(auto self=weak.lock()){
        auto& records=self->activeTranscriptRecords_;
        records.erase(std::remove_if(records.begin(),records.end(),
          [&pending](const auto& value){return value.samePromise(pending);}),
          records.end());
        if(records.empty()){
          auto waiters=std::move(self->transcriptIdleWaiters_);
          self->transcriptIdleWaiters_.clear();
          for(const auto& waiter:waiters)waiter.resolve({});
        }
        if(error){self->inputs_.onDetachedError(error);return;}
        if(seq==self->transcriptCallSeq_&&last&&*last&&!incremental)
          self->lastRecordedParent_=*last;
      }
    });
    if(incremental||wasFirstRender||sameHeadShrink){
      if(auto last=persistence.lastChainParticipant(slice,*messages))
        lastRecordedParent_=std::move(last);
    }
    lastRecordedLength_=messages->size();
    firstRecordedUuid_=std::move(first);
  }catch(...){inputs_.onDetachedError(std::current_exception());}
}
void ApplicationRuntime::acceptStep(const query::AsyncEngineStep& step){
  if(!step.event)return;
  const auto& messages=inputs_.presentation.messages;
  const auto beforeSize=messages.size();
  const auto beforeLast=messages.empty()?interactive::MessageRef{}:messages.back();
  interactive::StreamInput input{step.event,step.tombstoneTarget};
  if(streamSink_)streamSink_(std::move(input));
  else{
    interactive::applyStreamMessage(inputs_.presentation,input,
      inputs_.presentationImports);
    requestRedraw();
  }
  publishMessageChanges(beforeSize,beforeLast);
}
} // namespace batbox::application_runtime
