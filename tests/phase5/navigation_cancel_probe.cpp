#include "interactive/navigation/Cancel.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
using namespace batbox::interactive::navigation;
struct Case {
  std::optional<std::string> dialog,streaming;
  bool remote=false,proactive=false,budget=false;
  Case(std::optional<std::string> focus={},
    std::optional<std::string> stream={},bool isRemote=false,
    bool isProactive=false,bool hasBudget=false):dialog(std::move(focus)),
    streaming(std::move(stream)),remote(isRemote),proactive(isProactive),
    budget(hasBudget){}
};
Json run(const Case& test){
  event_loop::Loop loop;
  QueryTurnState turn;
  turn.guard.reserve();turn.skipIdleCheck=true;
  std::vector<std::string> trace;
  auto listener=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("guard");
  });
  auto unsubscribe=turn.guard.subscribe(listener);
  (void)unsubscribe;
  PresentationState presentation;
  presentation.streamingText=test.streaming;
  auto first=std::make_shared<Json>(Json{{"type","user"}});
  MessageServices messageServices;
  messageServices.publishMessages=[&](const TurnMessagesRef& value){
    trace.push_back("messages:"+std::to_string(value->size()));
  };
  MessageState messages(std::make_shared<TurnMessages>(TurnMessages{first}),
    messageServices);
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  auto store=std::make_shared<app_state::AppStateStore>(app);
  LoadingServices loadingServices;
  loadingServices.store=store;loadingServices.nowMs=[]{return 100.0;};
  loadingServices.getTipToShowOnSpinner=[&](const TipContext&){
    trace.push_back("pick");
    event_loop::Deferred<TipRef> done(loop);done.resolve({});return done;
  };
  loadingServices.endInteractionSpan=[&]{trace.push_back("end");};
  loadingServices.clearSpeculativeChecks=[&]{trace.push_back("clear");};
  loadingServices.onDetachedError=[](std::exception_ptr error){
    std::rethrow_exception(error);
  };
  auto loading=std::make_shared<LoadingState>(loop,turn.guard,presentation,
    messages,std::move(loadingServices));
  CancelState state;
  state.focusedInputDialog=test.dialog;
  state.streamMode="default";
  state.activeRemoteMode=test.remote;
  auto abort=std::make_shared<TurnAbort>();
  std::stop_callback abortListener(abort->controller.get_token(),[&]{
    trace.push_back("abort:user-cancel");
  });
  state.abortController=abort;
  auto gadget=std::make_shared<GadgetConfirmation>();
  gadget->onAbort=[&]{trace.push_back("tool-abort");};
  state.gadgetConfirms.push_back(gadget);
  for(int i=1;i<=2;++i){
    auto poetry=std::make_shared<PendingPoetry>();
    poetry->reject=[&,i](std::exception_ptr error){
      try{std::rethrow_exception(error);}catch(const std::exception& value){
        trace.push_back("reject:"+std::to_string(i)+":"+value.what());
      }
    };
    state.poetryEntries.push_back(poetry);
  }
  CancelServices services(loop,turn,messages,*loading,presentation);
  services.logForDebugging=[&](const std::string& value){
    trace.push_back("log:"+value);
  };
  services.feature=[&](const std::string& name){
    return name=="PROACTIVE"?test.proactive:
      (name=="TOKEN_BUDGET"?test.budget:false);
  };
  services.pauseProactive=[&]{trace.push_back("pause");};
  services.cancelRemote=[&]{trace.push_back("remote-cancel");};
  services.createAssistantMessage=[&](const std::string& value){
    trace.push_back("assistant:"+value);
    return std::make_shared<Json>(Json{{"type","assistant"},
      {"message",{{"content",value}}}});
  };
  services.snapshotOutputTokensForTurn=[&](std::optional<double> value){
    trace.push_back(value?"budget:value":"budget:null");
  };
  services.publishGadgetConfirmQueue=[&]{trace.push_back("tool-clear");};
  services.publishPoetryQueue=[&]{trace.push_back("prompt-clear");};
  services.publishAbortController=[&](std::shared_ptr<TurnAbort> value){
    trace.push_back(value?"controller:value":"controller:null");
  };
  services.mrOnTurnComplete=[&](const TurnMessagesRef& value,bool aborted){
    trace.push_back("complete:"+std::to_string(value->size())+":"+
      (aborted?"true":"false"));
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  onCancel(state,services);
  loop.runReady();
  return Json{{"trace",trace},{"messages",messages.messages()->size()},
    {"guardActive",turn.guard.isActive()},{"skipIdle",turn.skipIdleCheck},
    {"abortReason",abort->reason.is_discarded()?Json():abort->reason},
    {"abortPresent",bool(state.abortController)},
    {"prompts",state.poetryEntries.size()},{"tools",state.gadgetConfirms.size()},
    {"streaming",presentation.streamingText?Json(*presentation.streamingText):Json()}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  const std::vector<Case> cases={
    {"elicitation"},{"tool-permission"},{"prompt"},
    {std::nullopt,std::nullopt,true},{},
    {std::nullopt,"  partial\n",false,true,true},
    {"prompt","   ",false,true,false}
  };
  for(const auto& item:cases)std::cout<<run(item).dump()<<'\n';
}
