#include "interactive/InteractionController.hpp"
#include "interactive/text_input/TextEditorIndigenous.hpp"
#include "text_width/IndigenousLayout.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace ix=batbox::interactive;
namespace ti=batbox::interactive::text_input;
static void expect(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
int main(){
  event_loop::Loop owner;
  ix::PresentationState state;
  auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());
  auto width=std::make_shared<text_width::StringWidth>(
    text_width::indigenousServices(intl));
  auto wrap=text_width::indigenousWrapAnsiModule().wrapAnsi();
  ti::KillRing ring;
  ti::EditorOptions editorOptions;
  editorOptions.columns=40;editorOptions.cursorChar=" ";
  std::vector<std::string> events;
  ti::EditorCallbacks editorCallbacks;
  editorCallbacks.onChange=[&](const std::string& text){
    events.push_back("change:"+text);};
  editorCallbacks.onOffsetChange=[&](std::size_t offset){
    events.push_back("offset:"+std::to_string(offset));};
  editorCallbacks.onSubmit=[&](const std::string& text){
    events.push_back("submit:"+text);};
  editorCallbacks.invert=[](const std::string& text){return text;};
  auto editor=std::make_shared<ti::TextEditor>(owner,ring,
    ti::indigenousMetricsServices(intl,width,wrap),editorOptions,
    ti::indigenousEditorCallbacks(std::move(editorCallbacks)));

  ix::InteractionImports imports;
  imports.editPoetryKey=ti::bindPoetryEditor(editor);
  imports.onTerminalResponse=[&](termio::ResponseRef response){
    expect(response&&response->is_object(),"terminal reply object");
    events.push_back("response");
  };
  imports.onMouse=[](keyboard_input::MouseRef){};
  imports.onTerminalFocus=[](bool){};
  imports.onSuspend=[]{throw std::logic_error("unexpected suspend");};
  imports.onQueueBlocked=[](bool){};
  imports.onCommandSettled=[](const ix::CommandDescriptor&,
                              const ix::CommandCompletion&,bool){};
  imports.onCommandError=[](const std::string& error){
    throw std::runtime_error(error);};
  std::string plain;
  imports.writePlain=[&](std::string_view text){plain+=text;};
  imports.onExit=[]{};
  terminal_ftxui::Options driverOptions;
  driverOptions.interactive=false;
  auto app=ix::InteractionController::create(owner,driverOptions,state,
                                             std::move(imports));
  app->start();
  app->onRawInput("a");
  app->onRawInput("b");
  app->onRawInput("\x1b[D");
  app->onRawInput("Z");
  expect(state.input.text=="aZb"&&state.input.caretUtf16==2,
         "raw input to native grapheme editor");
  app->onRawInput("\x7f");
  expect(state.input.text=="ab"&&state.input.caretUtf16==1,
         "raw backspace to editor");
  app->onRawInput("\x1b[?1;2c");
  app->onRawInput("\r");
  expect(events.back()=="submit:ab","native submit");
  expect(events[events.size()-2]=="response","response before submit");
  app->stop();
  const auto metrics=ti::indigenousMetricsServices(intl,width,wrap);
  for(const auto& chip:{std::string("[Pasted text #7 +40 lines]"),
                       std::string("[Pasted text #8 (4500 characters)]")}){
    const auto text="before "+chip+" after";
    const auto start=std::size_t{7},end=start+chip.size();
    auto before=ti::Cursor::fromText(text,80,metrics,start);
    auto after=ti::Cursor::fromText(text,80,metrics,end);
    expect(before.right().offset()==end,"right skips the whole pasted item");
    expect(after.left().offset()==start,"left skips the whole pasted item");
    expect(before.del().text()=="before  after","delete removes the whole pasted item");
    expect(after.backspace().text()=="before  after","backspace removes the whole pasted item");
    expect(before.nextWord().offset()==end,"word navigation cannot enter a pasted item");
    expect(after.prevWord().offset()==start,"backward word navigation skips pasted item");
  }
  std::cout<<"PASS native raw parser -> editor -> presentation; "
    <<events.size()<<" ordered callbacks\n";
}
