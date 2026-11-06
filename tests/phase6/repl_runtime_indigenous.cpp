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
  services.onDetachedError=[](std::exception_ptr error){if(error)
    std::rethrow_exception(error);};
  std::string highlight;
  TranscriptServices transcript;
  transcript.setHighlight=[&](const std::string& value){highlight=value;};
  transcript.clearPositions=[]{};
  transcript.cancelEditorTimer=[]{};
  auto runtime=std::make_shared<Runtime>(services,transcript);
  runtime->start();
  loop.runReady();
  assert(events[0]=="verify"&&events[1]=="memory");
  assert(stash->get("/tmp/BATBOX.md")->content=="raw");
  assert(stash->get("/tmp/BATBOX.md")->isPartialView);
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
  runtime->setTranscriptSearch("needle");
  assert(highlight=="needle");
  runtime->setTranscriptScreen(false,true);
  assert(highlight.empty());
  runtime->stop();
  std::cout<<"repl runtime integration passed\n";
}
