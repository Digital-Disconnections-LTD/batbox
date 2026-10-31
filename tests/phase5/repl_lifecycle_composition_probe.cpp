#include "interactive/lifecycle/BatSubmit.hpp"
#include "interactive/lifecycle/InitialMessage.hpp"
#include "interactive/lifecycle/LoadingState.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  event_loop::Loop loop;
  QueryGuard guard;
  PresentationState presentation;
  int redraws=0,queued=0;
  auto initial=std::make_shared<Json>(Json{{"type","user"},{"uuid","u1"},
    {"message",{{"content","hi"}}}});
  MessageState messages(std::make_shared<TurnMessages>(),
    bindPresentationMessages(presentation,[&]{++redraws;}));
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  app->fields->set("initialMessage",{{"message",*initial}});
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson({{"mode","default"}}));
  app->fields->set("toolPermissionContext",app->permissionContext->snapshot());
  auto store=std::make_shared<app_state::AppStateStore>(app);
  LoadingServices loadingServices;
  loadingServices.store=store;
  loadingServices.nowMs=[]{return 1.0;};
  loadingServices.getTipToShowOnSpinner=[&](const TipContext&){
    event_loop::Deferred<TipRef> result(loop);result.resolve({});return result;
  };
  loadingServices.endInteractionSpan=[]{};
  loadingServices.clearSpeculativeChecks=[]{};
  loadingServices.onDetachedError=[](std::exception_ptr error){
    std::rethrow_exception(error);
  };
  auto loading=std::make_shared<LoadingState>(loop,guard,presentation,
    messages,std::move(loadingServices));
  loading->start();
  InitialMessageServices initialServices(loop,store);
  initialServices.awaitPendingHooks=[&]{
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  initialServices.onSubmit=[&](const std::string& input){
    loading->beginSubmit();
    messages.setUserInputOnProcessing(input);
    guard.reserve();
    messages.setMessages(std::make_shared<TurnMessages>(TurnMessages{initial}));
    guard.cancelReservation();
    loading->resetLoadingState();
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  initialServices.fileHistoryEnabled=[]{return false;};
  initialServices.feature=[](const std::string&){return false;};
  initialServices.onDetachedError=[](std::exception_ptr error){
    std::rethrow_exception(error);
  };
  auto controller=std::make_shared<InitialMessageController>(
    std::move(initialServices));
  controller->observe(InitialMessage{initial,false,std::nullopt,
    Json(Json::value_t::discarded)},false);
  for(int i=0;i<6;++i)loop.runReady();
  auto bat=std::make_shared<BatSubmitServices>(loop);
  bat->createUserMessage=[](const std::string& text){
    return std::make_shared<Json>(Json{{"type","user"},
      {"message",{{"content",text}}}});
  };
  bat->appendMessageToLocalBat=[](const std::string&,TurnMessageRef){};
  bat->queuePendingMessage=[&](const std::string&,const std::string&){++queued;};
  bat->setInputValue=[](std::string){};
  BatSubmitHelpers helpers;
  helpers.setCursorOffset=[](std::size_t){};
  helpers.clearBuffer=[]{};
  onBatSubmit("follow up",BatSubmitMission{"a1","running",true},
    helpers,bat);
  const bool sameMessage=messages.messages()->size()==1&&
    messages.messages()->front()==initial&&
    presentation.messages.size()==1&&presentation.messages.front()==initial;
  const bool stateOkay=!messages.userMessagePending()&&
    !messages.processingInput()&& !presentation.loading&&
    loading->tipPickedThisTurn()&&queued==1&&redraws>0&&
    store->getState()->fieldRef("initialMessage")->is_null();
  const bool pass=sameMessage&&stateOkay;
  std::cout<<Json{{"pass",pass},{"sameMessage",sameMessage},
    {"queued",queued},{"redraws",redraws},
    {"processing",messages.processingInput().has_value()}}.dump()<<'\n';
  return pass?0:1;
}
