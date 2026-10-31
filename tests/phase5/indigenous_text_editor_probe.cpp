#include "interactive/text_input/TextEditor.hpp"
#include "intl/IndigenousInternal.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Unicode.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive::text_input;
using Out=nlohmann::ordered_json;

static MetricsServices metrics(){
  MetricsServices services;
  services.normalizeNfc=value::normalizeNFC;
  services.grapheme=intl::indigenousSegmenter("grapheme");
  services.word=intl::indigenousSegmenter("word");
  services.stringWidth=[g=services.grapheme](const std::string& text){
    double width=0;auto cursor=g->segment(text);
    while(const auto item=cursor.next())
      width+=static_cast<double>(value::stringLength(item->text));
    return width;
  };
  services.wrapAnsi=[](const std::string& text,double){return text;};
  return services;
}
struct Step {std::string input;terminal_events::Key key;int delta=100;};
static void scenario(const std::string& name,const std::string& initial,
                     std::vector<Step> steps,EditorOptions options,
                     bool filterBlocked=false,
                     std::optional<InlineGhostText> ghost={}){
  event_loop::Loop loop;
  KillRing ring;
  interactive::InputState state;
  state.text=initial;state.caretUtf16=value::stringLength(initial);
  Out trace=Out::array();
  double now=1000;
  EditorCallbacks callbacks;
  callbacks.onChange=[&](const std::string& value){
    trace.push_back(Out::array({"change",value}));};
  callbacks.onOffsetChange=[&](std::size_t offset){
    trace.push_back(Out::array({"offset",offset}));};
  callbacks.onSubmit=[&](const std::string& value){
    trace.push_back(Out::array({"submit",value}));};
  callbacks.onExit=[&]{trace.push_back(Out::array({"exit"}));};
  callbacks.onExitMessage=[&](bool show,const std::string& key){
    trace.push_back(Out::array({"exitMessage",show,key}));};
  callbacks.onHistoryUp=[&]{trace.push_back(Out::array({"historyUp"}));};
  callbacks.onHistoryDown=[&]{trace.push_back(Out::array({"historyDown"}));};
  callbacks.onHistoryReset=[&]{trace.push_back(Out::array({"historyReset"}));};
  callbacks.onClearInput=[&]{trace.push_back(Out::array({"clearInput"}));};
  callbacks.addToHistory=[&](const std::string& value){
    trace.push_back(Out::array({"addToHistory",value}));};
  callbacks.addNotification=[&](const NotificationRequest& request){
    trace.push_back(Out::array({"notify",request.key,request.text}));};
  callbacks.removeNotification=[&](const std::string& key){
    trace.push_back(Out::array({"removeNotification",key}));};
  callbacks.markBackslashReturnUsed=[&]{
    trace.push_back(Out::array({"backslashReturn"}));};
  callbacks.stripAnsi=[](const std::string& input){return input;};
  callbacks.invert=[](const std::string& input){return "<"+input+">";};
  callbacks.nowMs=[&]{return now;};
  if(filterBlocked)callbacks.inputFilter=[](const std::string& input,
                                            const terminal_events::Key&){
    return input=="blocked"?std::string():input;};
  TextEditor editor(loop,ring,metrics(),options,std::move(callbacks));
  for(const auto& step:steps){
    now+=step.delta;
    editor.onInput(step.input,step.key,state);
    trace.push_back(Out::array({"state",state.text,state.caretUtf16}));
  }
  const auto view=editor.view(state,ghost,
    ghost?std::function<std::string(const std::string&)>(
      [](const std::string& text){return "~"+text;}):
      std::function<std::string(const std::string&)>{});
  std::cout<<Out{{"name",name},{"trace",trace},
    {"view",Out::array({view.renderedValue,view.offset,view.cursorLine,
      static_cast<int>(view.cursorColumn),view.viewportCharOffset,
      view.viewportCharEnd})}}
    .dump()<<'\n';
}
int main(){
  EditorOptions options;options.columns=20;options.cursorChar="_";
  terminal_events::Key ctrl;ctrl.ctrl=true;
  terminal_events::Key left;left.leftArrow=true;
  terminal_events::Key right;right.rightArrow=true;
  terminal_events::Key home;home.home=true;
  terminal_events::Key del;del.deleteKey=true;
  scenario("kill-yank","abc def",{{"w",ctrl},{"y",ctrl},{"",left},
    {"Z",{}},{"",home},{"!",{}},{"",right},{"",del}},options);
  terminal_events::Key escape;escape.escape=true;
  scenario("escape-and-ctrl","answer",{{"",escape},{"",escape},
    {"c",ctrl},{"c",ctrl}},options);
  terminal_events::Key enter;enter.returnKey=true;
  auto shiftEnter=enter;shiftEnter.shift=true;
  auto lines=options;lines.multiline=true;
  scenario("enter-lines","ab\\",{{"\r",enter},{"Z",{}},
    {"\r",shiftEnter},{"o\r",{}},{"\r",enter}},lines);
  terminal_events::Key up;up.upArrow=true;
  terminal_events::Key down;down.downArrow=true;
  terminal_events::Key pageUp;pageUp.pageUp=true;
  terminal_events::Key tab;tab.tab=true;
  auto history=options;history.disableCursorMovementForUpDownKeys=true;
  scenario("filter-del-history","abc",{{"blocked",{}},
    {"\x7f\x7f",{}},{"",up},{"",down},{"",pageUp},{"",tab}},
    history,true);
  terminal_events::Key meta;meta.meta=true;
  scenario("yank-pop","one two",{{"w",ctrl},{"",left},{"k",ctrl},
    {"y",ctrl},{"y",meta}},options);
  scenario("empty-ctrl-d","",{{"d",ctrl},{"d",ctrl}},options);
  scenario("ghost-current","hi",{},options,false,InlineGhostText{"yo",2});
  scenario("ghost-stale","hi",{},options,false,InlineGhostText{"yo",0});
}
