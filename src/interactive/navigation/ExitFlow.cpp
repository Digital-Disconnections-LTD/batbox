#include "interactive/navigation/ExitFlow.hpp"
#include <stdexcept>

namespace batbox::interactive::navigation {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Exit binding required: ")+name);
}
}
void ExitFlow::setExiting(bool value){
  isExiting_=value;
  if(services_.publishExiting)services_.publishExiting(value);
}
void ExitFlow::setFlow(std::optional<UiRequest> value,bool undefined){
  flow_=std::move(value);
  flowUndefined_=undefined;
  if(services_.publishExitFlow)services_.publishExitFlow(flow_,flowUndefined_);
}
void ExitFlow::cancelFlow(){
  setFlow({});
  setExiting(false);
}
event_loop::Deferred<event_loop::Unit> ExitFlow::handleExit(){
  if(!services_.owner.isOwnerThread())throw std::logic_error("Exit owner required");
  event_loop::Deferred<event_loop::Unit> result(services_.owner);
  try{
    setExiting(true);
    need(services_.feature,"feature");
    if(services_.feature("BG_SESSIONS")){
      need(services_.isBgFlight,"isBgSession");
      if(services_.isBgFlight()){
        need(services_.detachTmuxClient,"detachTmuxClient");
        services_.detachTmuxClient();
        setExiting(false);
        result.resolve({});return result;
      }
    }
    need(services_.getCurrentWorktreeFlight,"getCurrentWorktreeSession");
    if(!services_.getCurrentWorktreeFlight().is_null()){
      need(services_.makeWorktreeExitRequest,"makeWorktreeExitRequest");
      setFlow(services_.makeWorktreeExitRequest());
      result.resolve({});return result;
    }
    need(services_.loadExit,"loadExit");
    auto loaded=services_.loadExit();
    if(!services_.owner.dispatcher().sameLoop(loaded.dispatcher()))
      throw std::invalid_argument("Exit loader Promise crossed owner loop");
    auto self=shared_from_this();
    loaded.then([self,result](const ExitServices::ExitCall* call,
        const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      try{
        if(!call||!*call)throw std::invalid_argument("Exit call required");
        auto done=(*call)([]{});
        if(!self->services_.owner.dispatcher().sameLoop(done.dispatcher()))
          throw std::invalid_argument("Exit-call Promise crossed owner loop");
        done.then([self,result](const ExitServices::Result* flow,
            const std::exception_ptr& failure){
          if(failure){result.reject(failure);return;}
          try{
            if(!flow)throw std::logic_error("Exit call result missing");
            self->setFlow(flow->view,!flow->isNull&&!flow->view);
            if(flow->isNull)self->setExiting(false);
            result.resolve({});
          }catch(...){result.reject(std::current_exception());}
        });
      }catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::interactive::navigation
