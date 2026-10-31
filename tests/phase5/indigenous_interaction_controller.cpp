#include "interactive/InteractionController.hpp"
#include "termio/Csi.hpp"
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

// Isolated composition fixture: the keyboard and presentation owners have
// their own focused checks. Production links their real implementations.
namespace batbox::keyboard_input {
ParseResult parseMultipleKeypresses(const KeyParseState&,const Input&){
  throw std::logic_error("Raw parser fixture import not bound");
}
}
namespace batbox::interactive {
void updateViewport(PresentationState& state,int columns,int rows){
  state.viewport.columns=columns;state.viewport.rows=rows;++state.revision;
}
void scrollViewport(PresentationState& state,std::ptrdiff_t delta,
                    std::size_t,std::size_t){
  state.viewport.topRow+=static_cast<std::size_t>(delta);
  state.viewport.follow=false;++state.revision;
}
void applyStreamMessage(PresentationState& state,const StreamInput&,
                        const PresentationImports&){++state.revision;}
}
namespace batbox::terminal_ftxui {
PresentationLayout layoutPresentation(const interactive::PresentationState&,
                                      int,int){
  PresentationLayout result;result.transcriptRowCount=30;result.bodyHeight=10;
  return result;
}
ftxui::Element renderPresentation(const interactive::PresentationState&,
                                  ftxui::Dimensions){return ftxui::text("fixture");}
std::string drainPresentationPlain(const interactive::PresentationState&,
                                   PlainCursor&){return {};}
}

using namespace batbox;
static keyboard_input::KeyRef key(const char* name,std::string sequence=""){
  auto result=std::make_shared<keyboard_input::ParsedKey>();
  result->name=name;result->sequence=std::move(sequence);
  return result;
}
static void expect(bool condition,const char* description){
  if(!condition)throw std::runtime_error(description);
}
int main(){
  event_loop::Loop owner;
  interactive::PresentationState state;
  std::vector<std::string> trace;
  int edits=0,responses=0,mouse=0,exits=0;
  auto response=std::make_shared<Json>(Json::object());
  interactive::InteractionImports imports;
  imports.editPoetryKey=[&](keyboard_input::KeyRef input,
                            interactive::InputState& view){
    ++edits;view.text+=input->sequence.value_or("");
  };
  imports.onTerminalResponse=[&](termio::ResponseRef received){
    expect(received==response,"response identity");++responses;
  };
  imports.onMouse=[&](keyboard_input::MouseRef){++mouse;};
  imports.onTerminalFocus=[&](bool focused){
    trace.push_back(focused?"terminal:focus":"terminal:blur");
  };
  imports.onSuspend=[]{throw std::logic_error("suspend not expected");};
  imports.onQueueBlocked=[&](bool blocked){
    trace.push_back(blocked?"queue:blocked":"queue:ready");
  };
  imports.onCommandSettled=[&](const interactive::CommandDescriptor&,
      const interactive::CommandCompletion& done,bool failed){
    expect(!failed&&done.result=="selected two","completion result");
    trace.push_back("settled");
  };
  imports.onCommandError=[](const std::string& error){
    throw std::runtime_error(error);
  };
  imports.writePlain=[](std::string_view){};
  imports.onExit=[&]{++exits;};
  terminal_ftxui::Options options;
  options.interactive=false;options.redirectedWidth=42;
  options.redirectedHeight=11;
  auto controller=interactive::InteractionController::create(
    owner,options,state,std::move(imports));
  controller->start();
  expect(state.viewport.columns==42&&state.viewport.rows==11,
         "redirected viewport");
  controller->onParsedInput(key("x","x"));
  expect(edits==1&&state.input.text=="x","prompt editor routing");
  controller->onParsedInput(key("focusout",termio::FOCUS_OUT));
  expect(!state.input.focused,"terminal blur");
  controller->onParsedInput(key("focusin",termio::FOCUS_IN));
  expect(state.input.focused,"terminal focus");
  controller->onParsedInput(keyboard_input::ParsedResponse{"response",response});
  expect(responses==1,"terminal response routing");
  controller->onParsedInput(std::make_shared<keyboard_input::ParsedMouse>());
  expect(mouse==1,"mouse routing");

  interactive::UiRequest modal;
  modal.kind=interactive::UiRequest::Kind::Select;
  modal.id="select";modal.choices={"one","two"};
  controller->commands().start({"choose","",false,false},[&](
      interactive::CommandUi::Done done,
      interactive::CommandUi::Ready ready,
      interactive::CommandUi::Error){
    ready(modal,[done=std::move(done)](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Activate){
        interactive::CommandCompletion result;
        result.result=action.index==1?"selected two":"wrong item";
        done(std::move(result));
      }
    });
  });
  expect(state.modal&&state.modal->id=="select"&&!state.input.focused,
         "modal presentation/focus");
  controller->onParsedInput(key("down","\x1b[B"));
  expect(state.modal&&state.modal->selectedIndex==1,
         "modal selection");
  controller->onParsedInput(key("return","\r"));
  owner.runReady();
  expect(!state.modal&&state.input.focused&&!controller->commands().busy(),
         "modal cleanup");
  controller->stop();
  expect(exits==1,"driver exit");
  const std::vector<std::string> expected={
    "terminal:blur","terminal:focus","queue:blocked",
    "settled","queue:ready"};
  expect(trace==expected,"event order");
  std::cout<<"Interaction controller: parser routing, modal selection, and focus pass\n";
}
