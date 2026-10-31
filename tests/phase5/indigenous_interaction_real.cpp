#include "interactive/InteractionController.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace batbox;
namespace ix=batbox::interactive;

static void expect(bool okay,const char* description){
  if(!okay)throw std::runtime_error(description);
}
int main(){
  event_loop::Loop owner;
  ix::PresentationState state;
  state.loading=true;
  std::vector<std::string> events;
  std::string plain;
  int terminalResponses=0,edits=0,exits=0;
  ix::InteractionImports imports;
  imports.presentation.stream.connectorTextEnabled=[]{return false;};
  imports.presentation.stream.nowMs=[]{return 4000.0;};
  imports.presentation.newConversationId=[]{return "next-conversation";};
  imports.presentation.removeTranscriptMessage=[](const Json&){};
  imports.editPoetryKey=[&](keyboard_input::KeyRef key,ix::InputState& input){
    ++edits;
    input.text+=key->sequence.value_or("");
    input.caretUtf16=value::stringLength(input.text);
  };
  imports.onTerminalResponse=[&](termio::ResponseRef response){
    expect(response&&response->is_object(),"terminal reply object");
    ++terminalResponses;
    events.push_back("response");
  };
  imports.onMouse=[](keyboard_input::MouseRef){};
  imports.onTerminalFocus=[&](bool focused){
    events.push_back(focused?"focus":"blur");
  };
  imports.onSuspend=[]{throw std::logic_error("suspend not expected");};
  imports.onQueueBlocked=[&](bool blocked){
    events.push_back(blocked?"blocked":"ready");
  };
  imports.onCommandSettled=[&](const ix::CommandDescriptor&,
      const ix::CommandCompletion& completion,bool failed){
    expect(!failed&&completion.result=="selected two",
           "native command result");
    events.push_back("settled");
  };
  imports.onCommandError=[](const std::string& error){
    throw std::runtime_error(error);
  };
  imports.writePlain=[&](std::string_view bytes){plain+=bytes;};
  imports.onExit=[&]{++exits;};
  terminal_ftxui::Options options;
  options.interactive=false;
  options.redirectedWidth=42;options.redirectedHeight=11;
  auto app=ix::InteractionController::create(owner,options,state,
                                             std::move(imports));
  app->start();
  app->onRawInput("h");
  app->onRawInput("i");
  expect(state.input.text=="hi"&&edits==2,"raw bytes to prompt editor");
  app->onRawInput("\x1b[?1;2c");
  expect(terminalResponses==1,"raw terminal response");

  auto delta=std::make_shared<Json>(Json{{"type","stream_event"},
    {"event",{{"type","content_block_delta"},{"index",0},
      {"delta",{{"type","text_delta"},{"text","Hello"}}}}}});
  app->applyStream({delta,{}});
  expect(state.streamingText=="Hello"&&state.responseLengthUtf16==5,
         "real stream reducer");
  auto final=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a1"},
    {"message",{{"content",Json::array({
      Json{{"type","text"},{"text","Hello"}}})}}}});
  app->applyStream({final,{}});
  expect(state.messages.size()==1&&state.messages.front()==final&&
         !state.streamingText&&plain=="Hello\n",
         "real presentation and plain drain");

  ix::UiRequest modal;
  modal.kind=ix::UiRequest::Kind::Select;
  modal.id="choice";modal.title="Choose";
  modal.choices={"one","two"};
  app->commands().start({"choose","",false,false},[&](
      ix::CommandUi::Done done,ix::CommandUi::Ready ready,
      ix::CommandUi::Error){
    ready(modal,[done=std::move(done)](const ix::UiAction& action){
      if(action.kind==ix::UiAction::Kind::Activate){
        ix::CommandCompletion completion;
        completion.result=action.index==1?"selected two":"wrong item";
        done(std::move(completion));
      }
    });
  });
  expect(state.modal&&!state.input.focused,"native modal focus");
  app->onRawInput("\x1b[B");
  expect(state.modal&&state.modal->selectedIndex==1,"raw modal arrow");
  app->onRawInput("\r");
  owner.runReady();
  expect(!state.modal&&state.input.focused&&!app->commands().queueBlocked(),
         "native modal completion cleanup");
  app->stop();
  expect(exits==1,"native driver exit");
  const std::vector<std::string> expected={
    "response","blocked","settled","ready"};
  expect(events==expected,"native event order");
  std::cout<<"Real FTXUI composition: raw parser, stream reducer, modal and output pass\n";
}
