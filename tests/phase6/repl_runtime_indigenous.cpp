#include "interactive/repl_runtime/Runtime.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::repl_runtime;

int main(){
  event_loop::Loop loop;
  QueryGuard guard;
  submit::CommandQueue queue(loop);
  auto messages=std::make_shared<TurnMessages>();
  lifecycle::MessageState messageState(messages);
  PresentationState presentation;
  auto stash=file_state_stash::createFileStateStashWithSizeLimit(100);
  RuntimeServices services(loop,guard,queue,messageState,presentation,stash);
  std::vector<std::string> events;
  auto done=[&loop]{event_loop::Deferred<event_loop::Unit> d(loop);
    d.resolve({});return d;};
  services.reverifyApiKey=[&]{events.push_back("verify");return done();};
  services.shutdownDiagnostics=[&]{events.push_back("shutdown");return done();};
  services.getMemoryFiles=[&]{
    events.push_back("memory");
    event_loop::Deferred<std::vector<MemoryFile>> d(loop);
    d.resolve({MemoryFile{"memory","/tmp/BATBOX.md","injected",
      {},std::string("raw"),true}});
    return d;
  };
  services.nowMs=[]{return 123.0;};
  services.logForDebugging=[&](const std::string& value){
    events.push_back(value);
  };
  const std::vector<user_input::CommandEntry> commands;
  services.submitServices=std::make_shared<submit::SubmitServices>(
    submit::SubmitServices{loop,guard,queue,commands});
  services.makeQueuedRequest=[](const submit::CommandList&){
    return submit::PoetrySubmitRequest{};
  };
  bool activeLocalUi=false;
  services.hasActiveLocalUi=[&]{return activeLocalUi;};
  services.createUserMessage=[](const std::string& content,
      std::optional<bool> isMeta){
    Json value={{"type","user"},{"content",content}};
    if(isMeta)value["isMeta"]=*isMeta;
    return std::make_shared<Json>(std::move(value));
  };
  std::shared_ptr<TurnAbort> currentAbort;
  services.setAbortController=[&](std::shared_ptr<TurnAbort> value){
    currentAbort=std::move(value);
  };
  services.getAbortController=[&]{return currentAbort;};
  services.onQuery=[&](TurnRequest request){
    assert(request.newMessages->size()==1);
    assert(request.abort==currentAbort);
    assert(request.shouldQuery&&request.mainLoopModel=="model");
    events.push_back("query");return done();
  };
  services.mainLoopModel=[]{return "model";};
  auto kept=std::make_shared<Json>(Json{{"name","keep"}});
  auto safe=std::make_shared<Json>(Json{{"name","safe"}});
  auto dropped=std::make_shared<Json>(Json{{"name","drop"}});
  services.initialLocalCommands={kept,safe,dropped};
  services.isRemoteSafeCommand=[safe](const LocalCommandRef& item){
    return item==safe;
  };
  services.publishLocalCommands=[&](const LocalCommands& list){
    assert(list.size()==2&&list[0]==kept&&list[1]==safe);
  };
  services.recordUserActivity=[&]{events.push_back("activity");};
  services.getSubmitCount=[]{return std::size_t{0};};
  services.updateLastInteractionTime=[&](bool immediate){
    assert(immediate);events.push_back("interaction");
  };
  services.startBackgroundHousekeeping=[&]{events.push_back("housekeeping");};
  services.onDetachedError=[](std::exception_ptr error){if(error)
    std::rethrow_exception(error);};
  std::string highlight;
  TranscriptServices transcript;
  transcript.setHighlight=[&](const std::string& value){highlight=value;};
  transcript.clearPositions=[]{};
  transcript.cancelEditorTimer=[]{};
  IdleServices idle;
  idle.messageIdleThresholdMs=[]{return 0.0;};
  idle.getLastInteractionTime=[]{return 0.0;};
  idle.nowMs=[]{return 124.0;};
  idle.focusedDialogPresent=[]{return false;};
  idle.sendIdlePoetry=[&](const std::string& message,
      const std::string& type){
    assert(message=="Batbox is waiting for your input");
    assert(type=="idle_prompt");
    events.push_back("idle");
  };
  idle.idleReturnMode=[]{return "off";};
  auto runtime=std::make_shared<Runtime>(services,transcript,idle);
  runtime->start();
  loop.runReady();
  assert(events[0]=="activity"&&events[1]=="interaction");
  assert(events[2]=="verify"&&events[3]=="memory");
  assert(stash->get("/tmp/BATBOX.md")->content=="raw");
  assert(stash->get("/tmp/BATBOX.md")->isPartialView);
  runtime->handleRemoteInit({"keep"});
  assert(!guard.isActive());
  assert(runtime->handleIncomingPoetry("hello",true));
  assert((*currentAbort).aborted()==false);
  loop.runReady();
  activeLocalUi=true;
  queue.enqueue(Json{{"mode","custom"},{"priority","now"}});
  runtime->observeQueuedCommands();
  assert(currentAbort->aborted()&&currentAbort->reason=="interrupt");
  auto one=std::make_shared<Json>(Json{{"type","user"},{"content","one"}});
  messageState.setMessages(std::make_shared<TurnMessages>(TurnMessages{one}));
  presentation.streamingGadgetUses.resize(1);
  runtime->handleEnterTranscript();
  auto two=std::make_shared<Json>(Json{{"type","assistant"}});
  messageState.setMessages(std::make_shared<TurnMessages>(TurnMessages{one,two}));
  presentation.streamingGadgetUses.resize(2);
  assert(runtime->transcriptMessages().size()==1);
  assert(runtime->transcriptMessages()[0]==one);
  assert(runtime->transcriptStreamingGadgetUsesLength()==1);
  runtime->handleExitTranscript();
  assert(runtime->transcriptMessages().size()==2);
  runtime->setTranscriptScreen(true,true);
  runtime->setTranscriptSearch("needle",true,2,0);
  assert(highlight=="needle");
  runtime->setTranscriptScreen(false,true);
  assert(highlight.empty()&&runtime->editorGeneration()>0);
  runtime->observeIdleEffects(123,false,false,1);
  loop.run();
  assert(events.back()=="idle");
  runtime->stop();
  std::cout<<"repl runtime integration passed\n";
}
