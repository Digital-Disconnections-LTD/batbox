#include "interactive/navigation/Cancel.hpp"
#include "interactive/navigation/ExitFlow.hpp"
#include "interactive/navigation/Rewind.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
using namespace batbox::interactive::navigation;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  event_loop::Loop loop;
  QueryTurnState turn;turn.guard.reserve();turn.skipIdleCheck=true;
  PresentationState presentation;
  presentation.streamingText="partial";
  auto user=std::make_shared<Json>(Json{{"type","user"},{"uuid","u1"},
    {"message",{{"content","draft"}}}});
  MessageState messages(std::make_shared<TurnMessages>(TurnMessages{user}));
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson({{"mode","default"}}));
  app->fields->set("toolPermissionContext",app->permissionContext->snapshot());
  auto store=std::make_shared<app_state::AppStateStore>(app);
  LoadingServices loadingServices;
  loadingServices.store=store;loadingServices.nowMs=[]{return 100.0;};
  loadingServices.getTipToShowOnSpinner=[&](const TipContext&){
    event_loop::Deferred<TipRef> done(loop);done.resolve({});return done;
  };
  loadingServices.endInteractionSpan=[]{};
  loadingServices.clearSpeculativeChecks=[]{};
  loadingServices.onDetachedError=[](std::exception_ptr error){
    std::rethrow_exception(error);
  };
  auto loading=std::make_shared<LoadingState>(loop,turn.guard,presentation,
    messages,std::move(loadingServices));
  int repins=0,gadgetAborts=0,detachments=0;
  DialogFocus focus(*loading,[&]{++repins;});
  DialogInputs dialog;dialog.gadgetPermission=true;dialog.isLoading=true;
  focus.observe(dialog,100);
  CancelState cancel;
  cancel.focusedInputDialog=focus.current();
  cancel.streamMode="default";
  auto gadget=std::make_shared<GadgetConfirmation>();
  gadget->onAbort=[&]{++gadgetAborts;};cancel.gadgetConfirms.push_back(gadget);
  cancel.abortController=std::make_shared<TurnAbort>();
  const auto abort=cancel.abortController;
  CancelServices cancelServices(loop,turn,messages,*loading,presentation);
  cancelServices.logForDebugging=[](const std::string&){};
  cancelServices.feature=[](const std::string&){return false;};
  cancelServices.createAssistantMessage=[](const std::string& text){
    return std::make_shared<Json>(Json{{"type","assistant"},
      {"message",{{"content",text}}}});
  };
  cancelServices.mrOnTurnComplete=[&](const TurnMessagesRef&,bool){
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  onCancel(cancel,cancelServices);
  const bool partialBeforeRestore=messages.messages()->size()==2&&
    messages.messages()->front()==user&&
    messages.messages()->back()!=user&&
    (*messages.messages()->back())["message"]["content"]=="partial";
  submit::CommandQueue queue(loop);
  queue.enqueue(Json{{"mode","prompt"},{"value","queued"}});
  std::string input="draft",mode="prompt";
  QueueCancelServices queueServices(queue);
  queueServices.nowMs=[]{return 100.0;};
  queueServices.setInputValue=[&](const std::string& text){input=text;};
  queueServices.setInputMode=[&](const std::string& value){mode=value;};
  handleQueuedCommandOnCancel(input,queueServices);
  const bool queueRestored=input=="queued\ndraft"&&queue.empty();
  RewindServices rewindServices(loop,messages,store);
  rewindServices.randomUUID=[]{return "s2";};
  rewindServices.setConversationId=[](const std::string&){};
  rewindServices.resetMicrocompactState=[]{};
  rewindServices.feature=[](const std::string&){return false;};
  rewindServices.setInputValue=[&](const std::string& text){input=text;};
  rewindServices.setInputMode=[&](const std::string& value){mode=value;};
  auto rewind=std::make_shared<Rewind>(std::move(rewindServices));
  rewind->handleRestoreMessage(user);
  const bool deferred=messages.messages()->size()==2;
  for(int index=0;index<4;++index)loop.runReady();
  ExitServices exitServices(loop);
  exitServices.feature=[](const std::string& name){return name=="BG_SESSIONS";};
  exitServices.isBgFlight=[]{return true;};
  exitServices.detachTmuxClient=[&]{++detachments;};
  auto exit=std::make_shared<ExitFlow>(std::move(exitServices));
  auto completed=exit->handleExit();
  loop.runReady();
  const bool pass=partialBeforeRestore&&queueRestored&&deferred&&
    !turn.guard.isActive()&&!turn.skipIdleCheck&&gadgetAborts==1&&
    !abort->aborted()&&messages.messages()->empty()&&input=="draft"&&
    mode=="prompt"&&repins==0&&detachments==1&&
    !exit->isExiting()&&completed.settled();
  std::cout<<Json{{"pass",pass},{"partialBeforeRestore",partialBeforeRestore},
    {"queueRestored",queueRestored},{"deferred",deferred},
    {"toolAborts",gadgetAborts},{"messages",messages.messages()->size()},
    {"input",input},{"detachments",detachments}}.dump()<<'\n';
  return pass?0:1;
}
