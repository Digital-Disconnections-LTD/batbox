#include "interactive/lifecycle/InitialMessage.hpp"
#include <stdexcept>

namespace batbox::interactive::lifecycle {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Initial-message binding required: ")+name);
}
const Json& field(const Json& value,const char* name){return value::property(value,name);}
bool hasPlanContent(const InitialMessage& initial){
  return initial.message&&value::truthy(field(*initial.message,"planContent"));
}
std::string messageId(const InitialMessage& initial){
  if(!initial.message)return {};
  const auto& uuid=field(*initial.message,"uuid");
  return uuid.is_string()?uuid.get<std::string>():std::string();
}
}
InitialMessageController::InitialMessageController(InitialMessageServices services):
  services_(std::move(services)){
  need(services_.store,"store");
  need(services_.awaitPendingHooks,"awaitPendingHooks");
  need(services_.onDetachedError,"onDetachedError");
}
void InitialMessageController::detached(std::exception_ptr error){
  services_.onDetachedError(error);
}
void InitialMessageController::observe(
    const std::optional<InitialMessage>& pending,bool isLoading){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Initial-message owner required");
  if(!pending||isLoading||processing_)return;
  processing_=true;
  try{process(*pending);}catch(...){detached(std::current_exception());}
}
void InitialMessageController::process(InitialMessage initial){
  if(!initial.message)throw std::invalid_argument("Initial message ref required");
  if(!initial.clearContext){afterClear(std::move(initial));return;}
  std::optional<std::string> slug;
  if(hasPlanContent(initial)){
    need(services_.getPlanSlug,"getPlanSlug");
    slug=services_.getPlanSlug();
  }
  need(services_.clearConversation,"clearConversation");
  auto clear=services_.clearConversation();
  if(!services_.owner.dispatcher().sameLoop(clear.dispatcher()))
    throw std::invalid_argument("Clear-conversation Promise crossed owner loop");
  auto self=shared_from_this();
  clear.then([self,initial=std::move(initial),slug](const event_loop::Unit*,
      const std::exception_ptr& error){
    if(error){self->detached(error);return;}
    try{
      need(self->services_.resetHaikuTitle,"resetHaikuTitle");
      self->services_.resetHaikuTitle();
      need(self->services_.clearBashGadgets,"clearBashTools");
      self->services_.clearBashGadgets();
      if(slug&& !slug->empty()){
        need(self->services_.getFlightId,"getSessionId");
        need(self->services_.setPlanSlug,"setPlanSlug");
        self->services_.setPlanSlug(self->services_.getFlightId(),*slug);
      }
      self->afterClear(initial);
    }catch(...){self->detached(std::current_exception());}
  });
}
void InitialMessageController::afterClear(InitialMessage initial){
  const auto mode=initial.mode;
  const auto allowed=initial.allowedPoetryEntries;
  services_.store->setState([this,mode,allowed](const app_state::AppStateRef& previous){
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    auto permission=previous->permissionContext;
    if(mode&& !mode->empty()){
      need(services_.buildPermissionUpdates,"buildPermissionUpdates");
      need(services_.applyPermissionUpdates,"applyPermissionUpdates");
      permission=services_.applyPermissionUpdates(permission,
        services_.buildPermissionUpdates(*mode,allowed));
      need(permission,"updated permission context");
    }
    if(services_.feature&&services_.feature("TRANSCRIPT_CLASSIFIER")&&
        mode&&*mode=="auto"){
      need(services_.stripDangerousPermissionsForAutoMode,
        "stripDangerousPermissionsForAutoMode");
      auto input=std::make_shared<value::SharedRecord>(permission->shallowCopy());
      input->set("mode","auto");
      input->set("prePlanMode",Json(Json::value_t::discarded));
      permission=services_.stripDangerousPermissionsForAutoMode(input);
      need(permission,"stripped permission context");
    }
    next->fields->set("initialMessage",nullptr);
    if(permission!=previous->permissionContext)
      next->setPermissionContext(permission);
    return next;
  });

  if(services_.fileHistoryEnabled&&services_.fileHistoryEnabled()){
    need(services_.fileHistoryMakeSnapshot,"fileHistoryMakeSnapshot");
    auto store=services_.store;
    InitialMessageServices::HistorySetter setter=[store](
        const InitialMessageServices::HistoryUpdate& update){
      store->setState([&update](const app_state::AppStateRef& previous){
        auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
        next->fields->setRef("fileHistory",update(previous->fieldRef("fileHistory")));
        return next;
      });
    };
    const auto history=services_.fileHistoryMakeSnapshot(setter,messageId(initial));
    if(!services_.owner.dispatcher().sameLoop(history.dispatcher()))
      throw std::invalid_argument("File-history Promise crossed owner loop");
    auto self=shared_from_this();
    history.then([self](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)self->detached(error);
    });
  }
  auto pending=services_.awaitPendingHooks();
  if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Pending-hook Promise crossed owner loop");
  auto self=shared_from_this();
  pending.then([self,initial=std::move(initial)](const event_loop::Unit*,
      const std::exception_ptr& error){
    if(error){self->detached(error);return;}
    try{self->afterHooks(initial);}
    catch(...){self->detached(std::current_exception());}
  });
}
void InitialMessageController::afterHooks(InitialMessage initial){
  const auto& content=field(field(*initial.message,"message"),"content");
  event_loop::Deferred<event_loop::Unit> pending(services_.owner);
  if(content.is_string()&&!hasPlanContent(initial)){
    need(services_.onSubmit,"onSubmit");
    pending=services_.onSubmit(content.get<std::string>());
  }else{
    need(services_.setAbortController,"setAbortController");
    need(services_.onQuery,"onQuery");
    need(services_.mainLoopModel,"mainLoopModel");
    auto abort=std::make_shared<TurnAbort>();
    services_.setAbortController(abort);
    TurnRequest request;
    request.newMessages=std::make_shared<TurnMessages>(
      TurnMessages{initial.message});
    request.abort=std::move(abort);
    request.shouldQuery=true;
    request.mainLoopModel=services_.mainLoopModel();
    pending=services_.onQuery(std::move(request));
  }
  if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Initial dispatch Promise crossed owner loop");
  auto self=shared_from_this();
  pending.then([self](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)self->detached(error);
  });
  release_=services_.owner.setTimeout(100,[weak=weak_from_this()]{
    if(auto self=weak.lock())self->processing_=false;
  });
}
} // namespace batbox::interactive::lifecycle
