#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/presentation_providers/Chrome.hpp"
#include "interactive/presentation_providers/DefaultBindings.hpp"
#include "text_width/IndigenousLayout.hpp"
#include "history/References.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::conversation_shell;
namespace {
void check(bool condition,const std::string& message){
  if(!condition)throw std::logic_error(message);
}
}

int main(){
  const bool animate=false;
  event_loop::Loop loop;
  PresentationState state;
  QueryGuard guard;
  submit::CommandQueue queue(loop);
  const std::vector<user_input::CommandEntry> catalog;
  auto submit=std::make_shared<submit::SubmitServices>(
    submit::SubmitServices{loop,guard,queue,catalog});
  auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());
  auto width=std::make_shared<text_width::StringWidth>(
    text_width::indigenousServices(intl));
  auto wrap=text_width::indigenousWrapAnsiModule().wrapAnsi();
  messages::Services messageServices;
  messageServices.strictGadgetResultPairing=[]{return false;};
  messageServices.featureGate=[](const std::string&){return false;};
  messageServices.logError=[](const std::string&){};
  terminal_ftxui::Options options;
  options.mode=terminal_ftxui::Mode::Fullscreen;
  options.interactive=animate;
  options.redirectedWidth=40;options.redirectedHeight=12;
  Imports imports(loop,state,options);
  imports.submitServices=submit;
  imports.makeSubmitRequest=[](const std::string& text,const InputState&){
    submit::PoetrySubmitRequest value;value.input=text;return value;
  };
  imports.onRepin=[]{ };
  imports.getSubmitCount=[]{return std::size_t{0};};
  imports.copyMessageText=[](const std::string&){};
  imports.editMessage=[&](MessageRef){
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  imports.listSettings=[&](const PresentationState&){
    MessageListSettings settings;
    settings.gadgets=std::make_shared<GroupingGadgetSet>();
    settings.messageServices=&messageServices;
    settings.visibility.feature=[](const std::string&){return false;};
    settings.readSearch.classification.feature=[](const std::string&){return false;};
    settings.readSearch.classification.isFullscreen=[]{return true;};
    return settings;
  };
  bool showStartup=true;
  imports.surfaceOptions=[&](const PresentationState&){
    message_list_view::SurfaceOptions value;value.hideLogo=!showStartup;value.thinkingInPlace=true;return value;
  };
  imports.messageImports=[]{
    message_view::MessageImports value;
    value.fullscreen=[]{return true;};
    value.feature=[](const std::string&){return false;};
    value.isSnipBoundary=value.isSnipMarker=[](const Json&){return false;};
    value.isConnectorTextBlock=value.isAdvisorBlock=[](const Json&){return false;};
    value.logError=[](const std::string&){};
    return value;
  };
  imports.decisionImports=[]{return MessageDecisionImports{};};
  imports.projectionServices=[]{
    message_view::ProjectionServices value;
    value.system.eventMark=value.system.referenceMark=value.system.errorDot="●";
    value.system.renderSpecial=[](cards_system::SystemTextKind,const Json&,
        const cards_system::SystemTextCard& card,bool)->std::optional<ftxui::Element>{
      return ftxui::text(card.content);};
    return value;
  };
  imports.renderStreamingMarkdown=[](const std::string& text,bool){
    return ftxui::paragraph(text);
  };
  imports.chrome.renderHeader=[](const ViewState&){return ftxui::text("Batbox");};
  imports.chrome.renderFooter=[](const ViewState& view){
    return ftxui::text(view.searchOpen?"search":"ready");
  };
  presentation_providers::ThemeState theme({[]{return std::string("dark");},
    [](const std::string&){},[]{return std::string("dark");},[]{}});
  auto shortcuts=presentation_providers::makeShortcutProvider({"linux",true,
    [](const std::string&){return false;}},[]{return std::vector<presentation_providers::Binding>{};});
  auto chrome=presentation_providers::makeChromeProviders({theme,shortcuts,
    []{return std::string{};},[]{return std::string{};},
    []{return std::string{};},[]{return std::string{};}});
  imports.chrome.renderModal=chrome.renderModal;
  imports.chrome.renderPill=[](const ViewState&){return ftxui::text("Go to the action ↓");};
  imports.chrome.renderLogo=[]{return ftxui::text("Batbox");};
  imports.chrome.renderWelcome=[](const ViewState&){return ftxui::text("WELCOME_BAT");};
  imports.chrome.renderDivider=[](const message_list_view::DividerRequest&){
    return ftxui::text("---");
  };
  imports.editorMetrics=text_input::indigenousMetricsServices(intl,width,wrap);
  imports.editorCallbacks.invert=[](const std::string& text){return text;};
  imports.nowMs=[]{return 100.0;};
  imports.logForDebugging=[](const std::string&){};
  imports.terminalProgressEnabled=[]{return false;};
  imports.setTerminalProgress=[](std::optional<message_list_view::ProgressState>){};
  imports.onDetachedError=[](std::exception_ptr error){
    if(error)std::rethrow_exception(error);
  };
  int replies=0,settled=0;
  imports.input.onTerminalResponse=[&](termio::ResponseRef){++replies;};
  imports.input.onMouse=[](keyboard_input::MouseRef){};
  imports.input.onTerminalFocus=[](bool){};
  imports.input.onSuspend=[]{};
  imports.input.onQueueBlocked=[](bool){};
  imports.input.onCommandSettled=[&](const CommandDescriptor&,
      const CommandCompletion&,bool){++settled;};
  imports.input.onCommandError=[](const std::string& value){throw std::logic_error(value);};
  imports.input.writePlain=[](std::string_view){};
  imports.input.onExit=[]{};
  // Bat mission owner: a single live record the test edits between frames.
  // Fixture rows are notices so this shell needs no model-card services.
  const auto row=[](const std::string& uuid,const std::string& text){
    return Json{{"type","system"},{"subtype","informational"},{"level","notice"},
      {"isMeta",false},{"uuid",uuid},{"content",text}};
  };
  Json mission={{"type","local_agent"},{"agentType","general-purpose"},
    {"status","running"},{"description","Review parser"},{"retain",true},
    {"pendingMessages",Json::array()},
    {"messages",Json::array({
      row("a1","Starting")})}};
  std::vector<std::pair<std::string,std::string>> controls;
  bool controlsOpen=true;
  std::vector<std::string> notices,sent;
  std::optional<event_loop::Deferred<event_loop::Unit>> pendingSubmit;
  imports.openControlCommand=[&](const std::string& name,const std::string& args){
    controls.emplace_back(name,args);return controlsOpen;};
  // bat-2 was launched by bat-1; bat-3 is a second top-level Bat.
  Json others={
    {"bat-2",{{"type","local_agent"},{"agentType","general-purpose"},{"status","running"},
      {"description","Check lexer"},{"parentBatId","bat-1"},{"startTime",20},
      {"messages",Json::array({row("b1","Lexer notes")})}}},
    {"bat-3",{{"type","local_agent"},{"agentType","general-purpose"},{"status","running"},
      {"description","Write docs"},{"startTime",5},
      {"messages",Json::array({row("c1","Docs notes")})}}}};
  const auto lookup=[&](const std::string& id){return id=="bat-1"?mission:others.at(id);};
  imports.missionSnapshot=[&]{
    Json all=others;all["bat-1"]=mission;all["bat-1"]["startTime"]=10;return all;};
  imports.observeBatMission=[&](const std::string& id){
    event_loop::Deferred<Json> result(loop);result.resolve(lookup(id));return result;};
  imports.batMissionSnapshot=[&](const std::string& id)->std::optional<Json>{return lookup(id);};
  imports.submitBatMessage=[&](const std::string&,const std::string& text){
    sent.push_back(text);pendingSubmit.emplace(loop);return *pendingSubmit;};
  imports.releaseBatMission=[](const std::string&){};
  imports.stopBatMission=[](const std::string&){};
  imports.editorCallbacks.addNotification=[&](const auto& notification){
    notices.push_back(notification.text);};
  auto shell=Shell::create(std::move(imports));
  auto turnMessages=std::make_shared<TurnMessages>();
  lifecycle::MessageState active(turnMessages);
  auto stash=file_state_stash::createFileStateStashWithSizeLimit(100);
  repl_runtime::RuntimeServices runtimeServices(loop,guard,queue,active,state,stash);
  auto done=[&]{event_loop::Deferred<event_loop::Unit> result(loop);
    result.resolve({});return result;};
  runtimeServices.reverifyApiKey=done;
  runtimeServices.shutdownDiagnostics=done;
  runtimeServices.getMemoryFiles=[&]{
    event_loop::Deferred<std::vector<repl_runtime::MemoryFile>> result(loop);
    result.resolve({});return result;
  };
  runtimeServices.nowMs=[]{return 100.0;};
  runtimeServices.logForDebugging=[](const std::string&){};
  runtimeServices.submitServices=submit;
  runtimeServices.makeQueuedRequest=[](const submit::CommandList&){
    return submit::PoetrySubmitRequest{};
  };
  runtimeServices.hasActiveLocalUi=[weak=std::weak_ptr<Shell>(shell)]{
    if(auto current=weak.lock())return current->commands().hasActiveLocalUi();
    return false;
  };
  runtimeServices.getSubmitCount=[]{return std::size_t{0};};
  runtimeServices.onDetachedError=[](std::exception_ptr error){
    if(error)std::rethrow_exception(error);
  };
  runtimeServices.recordUserActivity=[]{};
  runtimeServices.updateLastInteractionTime=[](bool){};
  repl_runtime::TranscriptServices transcript;
  transcript.setHighlight=[](const std::string&){};
  transcript.clearPositions=[]{};
  transcript.cancelEditorTimer=[]{};
  shell->bindRuntime(std::make_shared<repl_runtime::Runtime>(runtimeServices,
    transcript));
  shell->start();loop.runReady();

  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(80),ftxui::Dimension::Fixed(20));
  const auto paint=[&]{
    ftxui::Render(screen,shell->renderSnapshot());shell->commitFrame();loop.runReady();
  };
  const auto lastText=[&]{
    const auto& messages=shell->presentation().messages;
    return messages.empty()||!messages.back()?std::string{}:
      value::property(*messages.back(),"content").is_string()?
        value::property(*messages.back(),"content").get<std::string>():std::string{};
  };
  // Ctrl+E then Ctrl+U clears what a refused command left in the input.
  const auto type=[&](const std::string& text){
    shell->onRawInput("\x05\x15");shell->onRawInput(text);shell->onRawInput("\r");loop.runReady();};
  shell->onRawInput("\x14");loop.runReady();
  check(controls.empty()&&shell->viewState().missionsCollapsed,
    "Ctrl+T in the leader collapses the Missions list");
  shell->onRawInput("\x14");loop.runReady();
  check(!shell->viewState().missionsCollapsed,"Ctrl+T again expands it");
  shell->openBatConversation("bat-1");loop.runReady();paint();
  check(shell->activeBatId()=="bat-1"&&shell->presentation().messages.size()==1,
    "Opening a Bat shows its conversation");
  {
    const auto frame=screen.ToString();
    std::size_t brands=0;
    for(auto at=frame.find("Batbox");at!=std::string::npos;at=frame.find("Batbox",at+1))++brands;
    check(brands==1&&frame.find("WELCOME_BAT")==std::string::npos,
      "A Bat view shows the brand once, in its header:\n"+frame);
  }
  shell->onRawInput("\x14");loop.runReady();
  check(controls.empty()&&shell->viewState().missionsCollapsed&&shell->activeBatId()=="bat-1",
    "Ctrl+T inside a Bat collapses the list and stays in the Bat");
  shell->onRawInput("\x14");loop.runReady();
  type("/bats");
  check(controls.size()==1&&controls.back()==std::pair<std::string,std::string>{"bats",""},
    "/bats inside a Bat opens the Bats list");
  type("/agents reviewer");
  check(controls.back()==std::pair<std::string,std::string>{"agents","reviewer"},
    "Legacy names pass through to the alias-aware command lookup");
  controlsOpen=false;
  type("/missions");
  check(!notices.empty()&&notices.back().find("Could not open /missions right now")!=std::string::npos&&
    shell->presentation().input.text=="/missions",
    "A control command that cannot open must say so");
  controlsOpen=true;
  type("/compact");
  check(notices.back().starts_with("/compact runs in the leader conversation")&&
    notices.back().find("/bats")!=std::string::npos,
    "Leader-only commands must explain where they run");

  type("please also check tests");
  check(sent==std::vector<std::string>{"please also check tests"},"Follow-up must reach the Bat");
  pendingSubmit->resolve({});loop.runReady();
  mission["pendingMessages"]=Json::array({"please also check tests"});
  mission["messages"].push_back(row("a-echo","Still reading"));
  paint();
  check(lastText()=="1 message queued · delivered at the next Gadget round",
    "A follow-up to a running Bat shows as queued: "+lastText());
  check(screen.ToString().find("queued")!=std::string::npos,"The queued line must render");
  mission["pendingMessages"]=Json::array();paint();
  check(lastText()=="Queued message delivered · the Bat reads it this round",
    "Draining the queue flips the indicator to delivered: "+lastText());
  mission["messages"].push_back(row("q1","Reading your note"));
  mission["messages"].push_back(row("a2","Checked tests"));
  paint();
  check(value::property(*shell->presentation().messages.back(),"uuid")=="a2",
    "The delivered note clears once the Bat answers");

  mission["status"]="completed";paint();
  type("one more pass");
  check(sent.size()==2,"Follow-up to a settled Bat must be sent");
  paint();
  check(lastText()=="Resuming this Bat…"&&shell->presentation().loading,
    "Messaging a settled Bat shows that it is resuming: "+lastText());
  mission["status"]="running";pendingSubmit->resolve({});loop.runReady();paint();
  check(value::property(*shell->presentation().messages.back(),"uuid")=="a2",
    "The resuming note clears once the Bat runs again");
  mission["status"]="completed";mission["inputResumeError"]="capacity reached";paint();
  check(lastText()=="Could not resume this Bat: capacity reached",
    "A failed resume is shown in the Bat view");

  // Alt+Up/Down walk the Missions list; the input is home.
  mission["status"]="running";mission.erase("inputResumeError");
  shell->returnToLeader();loop.runReady();paint();
  const auto press=[&](const std::string& bytes){
    shell->onRawInput(bytes);loop.runReady();paint();};
  const auto lone=[&](const std::string& bytes){
    // Past the lone-Escape timeout, so the Escape stands alone.
    shell->onRawInput(bytes);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));loop.runReady();paint();};
  const auto at=[&]{return shell->activeBatId().value_or("home");};
  const auto status=[&]{
    auto line=ftxui::Screen::Create(ftxui::Dimension::Fixed(120),ftxui::Dimension::Fixed(1));
    auto view=shell->viewState();view.columns=120;
    ftxui::Render(line,chrome.renderStatus(view));
    std::string text;
    for(int x=0;x<120;++x){const auto& glyph=line.PixelAt(x,0).character;text+=glyph.empty()?" ":glyph;}
    return text;
  };
  check(at()=="home"&&status().find("Alt+↓ Bats · Ctrl+T list")!=std::string::npos,
    "Home names the way into the Bats:\n"+status());
  press("\x1b[1;3B");
  check(at()=="bat-1","Alt+Down from home opens the top Bat, found "+at());
  press("\x1b[1;3B");
  check(at()=="bat-2"&&shell->presentation().messages.size()==1,
    "Alt+Down reaches the sub-Bat under its parent, found "+at());
  check(shell->viewState().batTrail==std::vector<std::string>{"Review parser","Check lexer"}&&
    status().find("Flight › Review parser › Check lexer · Alt+↑↓ move · Alt+← home · Ctrl+T list")!=
      std::string::npos,"Breadcrumb names the way down:\n"+status());
  press("\x1b[1;3B");press("\x1b[1;3B");
  check(at()=="bat-3","Alt+Down stops at the last Bat, found "+at());
  press("\x1b[1;3A");
  check(at()=="bat-2","Alt+Up moves to the previous Bat, found "+at());
  lone("\x1b");
  check(at()=="bat-1","Esc climbs one level to the parent Bat, found "+at());
  press("\x1b[1;3B");press("\x1b" "b");
  check(at()=="home","Alt+Left (ESC b) goes home from a sub-Bat, found "+at());
  press("\x1b[1;3B");press("\x1b[1;3A");
  check(at()=="home","Alt+Up from the top Bat goes home, found "+at());
  press("\x1b\x1b[B");
  check(at()=="bat-1","ESC ESC [B (Option as Meta) is Alt+Down, found "+at());
  shell->onRawInput("two words");loop.runReady();
  press("\x1b[1;3D");
  check(at()=="bat-1"&&shell->presentation().input.caretUtf16==4,
    "With text in the input Alt+Left jumps a word and stays in the Bat");
  shell->onRawInput("\x05\x15");loop.runReady();
  press("\x1b[1;3D");
  check(at()=="home","Alt+Left with an empty input goes home");
  lone("\x1b");press("\x1b[B");
  check(at()=="home","Esc then a later Down must not move into a Bat");
  lone("\x1b");press("\x1b[A");
  check(at()=="home","Esc then a later Up must stay home");
  shell->stop();
  std::cout<<"Bat conversation view: Missions list toggle, Alt navigation, control commands, "
    "queued and resuming states\n";
  return 0;
}
