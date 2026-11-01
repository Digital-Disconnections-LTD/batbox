#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/presentation_providers/Chrome.hpp"
#include "interactive/presentation_providers/DefaultBindings.hpp"
#include "text_width/IndigenousLayout.hpp"
#include "history/References.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
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
  imports.observeBatMission=[&](const std::string&){
    event_loop::Deferred<Json> result(loop);result.resolve(mission);return result;};
  imports.batMissionSnapshot=[&](const std::string&)->std::optional<Json>{return mission;};
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
  check(controls.size()==1&&controls.back().first=="bats",
    "Ctrl+T in the leader must open the Bats list");
  shell->openBatConversation("bat-1");loop.runReady();paint();
  check(shell->activeBatId()=="bat-1"&&shell->presentation().messages.size()==1,
    "Opening a Bat shows its conversation");
  shell->onRawInput("\x14");loop.runReady();
  check(controls.size()==2&&controls.back().first=="bats",
    "Ctrl+T inside a Bat must open the Bats list to switch Bats");
  type("/bats");
  check(controls.size()==3&&controls.back()==std::pair<std::string,std::string>{"bats",""},
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
  shell->stop();
  std::cout<<"Bat conversation view: Ctrl+T, control commands, queued and resuming states\n";
  return 0;
}
