#include "application_runtime/ApplicationRuntime.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_runtime {
void ApplicationRuntime::bindSubmit(){
  submit_=std::make_shared<interactive::submit::SubmitServices>(
    interactive::submit::SubmitServices{inputs_.owner,turnState_->guard,
      *queue_,inputs_.commands,
      {},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}});
  inputs_.bindSubmitProviders(*submit_);
  auto weak=weak_from_this();
  submit_->setAbortController=[weak](std::shared_ptr<interactive::TurnAbort> abort){
    if(auto self=weak.lock())self->activeAbort_=std::move(abort);
  };
  submit_->setUserInputOnProcessing=[weak](std::optional<std::string> text){
    if(auto self=weak.lock())self->messages_->setUserInputOnProcessing(
      std::move(text));
  };
  submit_->clearLocalUi=[weak]{if(auto self=weak.lock()){
    if(self->modalSink_)self->modalSink_({});
    self->inputs_.presentation.modal.reset();
    ++self->inputs_.presentation.revision;
    self->requestRedraw();
  }};
  submit_->onDetachedError=inputs_.onDetachedError;
  interactive::bindSubmitQueryTurn(inputs_.owner,*submit_,turnState_,turn_);
  const auto startTurn=submit_->onQuery;
  submit_->onQuery=[weak,startTurn](interactive::TurnRequest request){
    auto self=weak.lock();
    if(!self)throw std::logic_error("Application runtime is unavailable");
    if(self->stopping_){
      event_loop::Deferred<event_loop::Unit> rejected(self->inputs_.owner);
      rejected.reject(std::make_exception_ptr(
        std::logic_error("Application runtime is stopping")));
      return rejected;
    }
    const auto generation=self->turnState_->guard.generation();
    if(self->inputs_.onTurnRequested)self->inputs_.onTurnRequested(request);
    auto pending=startTurn(std::move(request));
    if(self->turnState_->guard.isRunning()&&
       self->turnState_->guard.generation()!=generation){
      self->activeTurns_.push_back(pending);
      pending.then([weak,pending](const event_loop::Unit*,
          const std::exception_ptr&){
        if(auto live=weak.lock()){
          auto& turns=live->activeTurns_;
          turns.erase(std::remove_if(turns.begin(),turns.end(),
            [&pending](const auto& value){return value.samePromise(pending);}),
            turns.end());
        }
      });
    }
    return pending;
  };
}
void ApplicationRuntime::bindRuntime(){
  interactive::repl_runtime::RuntimeServices services(inputs_.owner,
    turnState_->guard,*queue_,*messages_,inputs_.presentation,
    inputs_.readFileState);
  inputs_.bindRuntimeProviders(services);
  auto weak=weak_from_this();
  services.submitServices=submit_;
  services.nowMs=inputs_.nowMs;
  services.onDetachedError=inputs_.onDetachedError;
  services.setAbortController=[weak](std::shared_ptr<interactive::TurnAbort> abort){
    if(auto self=weak.lock())self->activeAbort_=std::move(abort);
  };
  services.getAbortController=[weak]{
    if(auto self=weak.lock())return self->activeAbort_;
    return std::shared_ptr<interactive::TurnAbort>{};
  };
  services.onQuery=submit_->onQuery;
  services.mainLoopModel=[weak,store=inputs_.store]{
    const auto state=store->getState()->snapshot();
    for(const auto* field:{"mainLoopModelForSession","mainLoopModel"}){
      const auto& selected=value::property(state,field);
      if(selected.is_string()&&!selected.get<std::string>().empty())
        return selected.get<std::string>();
    }
    if(auto self=weak.lock())return self->inputs_.config.model;
    throw std::logic_error("Application runtime is unavailable");
  };
  runtime_=std::make_shared<interactive::repl_runtime::Runtime>(
    std::move(services),std::move(inputs_.transcript),std::move(inputs_.idle));
}
} // namespace batbox::application_runtime
