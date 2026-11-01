#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/presentation_providers/Chrome.hpp"
#include "interactive/presentation_providers/DefaultBindings.hpp"
#include "text_width/IndigenousLayout.hpp"
#include "history/References.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::conversation_shell;

int main(int argc,char** argv){
  const bool animate=argc==2&&std::string(argv[1])=="--welcome-animation";
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
  imports.messageImports=[]{return message_view::MessageImports{};};
  imports.decisionImports=[]{return MessageDecisionImports{};};
  imports.projectionServices=[]{return message_view::ProjectionServices{};};
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
  imports.input.onCommandError=[](const std::string&){assert(false);};
  imports.input.writePlain=[](std::string_view){};
  imports.input.onExit=[]{};
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
  if(animate){
    assert(shell->driver().interactive());
    assert(shell->viewState().welcomeFrame<welcomeAnimationFrames);
    const auto welcomeShown=[&]{
      auto canvas=ftxui::Screen::Create(
        ftxui::Dimension::Fixed(state.viewport.columns),
        ftxui::Dimension::Fixed(state.viewport.rows));
      ftxui::Render(canvas,shell->renderSnapshot());shell->commitFrame();
      return canvas.ToString().find("WELCOME_BAT")!=std::string::npos;
    };
    assert(welcomeShown());
    unsigned advanced=0;
    auto advancing=loop.setTimeout(350,[&]{
      advanced=shell->viewState().welcomeFrame;
      assert(advanced>0&&advanced<welcomeAnimationFrames);
      assert(welcomeShown());
    });
    auto settled=loop.setTimeout(1600,[&]{
      assert(shell->viewState().welcomeFrame==welcomeAnimationFrames);
      assert(welcomeShown());
      shell->onRawInput("\x1b[O");shell->onRawInput("\x1b[I");
      updateViewport(state,40,12);
      assert(shell->viewState().welcomeFrame==welcomeAnimationFrames);
      assert(welcomeShown());
    });
    auto finished=loop.setTimeout(1900,[&]{
      assert(advanced>0&&shell->viewState().welcomeFrame==welcomeAnimationFrames);
      shell->stop();
    });
    loop.run();
    assert(shell->viewState().welcomeFrame==welcomeAnimationFrames);
    shell->start();loop.runReady();
    assert(shell->viewState().welcomeFrame==welcomeAnimationFrames);
    assert(welcomeShown());shell->stop();
    std::cout<<"welcome animation advanced and settled without replay\n";
    return 0;
  }
  assert(shell->viewState().welcomeFrame==welcomeAnimationFrames);
  // Paste content remains out of the editable line and expands exactly once
  // when submitted; CRLF must not turn into double line breaks.
  const std::string pasted="Résumé — systems\r\n"+std::string(4500,'x')+"\r\n雪 🦇";
  const std::string normalized="Résumé — systems\n"+std::string(4500,'x')+"\n雪 🦇";
  shell->onRawInput("\x1b[200~"+pasted+"\x1b[201~");
  const auto chip=state.input.text;
  assert(chip.starts_with("[Pasted text #1 (")&&chip.ends_with(" characters)]"));
  assert(chip.size()<64&&state.input.pastedContents.size()==1);
  assert(history::expandPastedTextRefs(chip,state.input.pastedContents)==normalized);
  shell->onRawInput("\x1b[D");assert(state.input.caretUtf16==0);
  shell->onRawInput("\x1b[C");assert(state.input.caretUtf16==chip.size());
  shell->onRawInput("\x7f");assert(state.input.text.empty());
  // Two adjacent pastes keep independent IDs and delete independently.
  shell->onRawInput("\x1b[200~"+pasted+"\x1b[201~");
  const auto first=state.input.text;
  shell->onRawInput("\x1b[200~"+pasted+"\x1b[201~");
  assert(history::expandPastedTextRefs(state.input.text,state.input.pastedContents)==
    normalized+normalized);
  shell->onRawInput("\x7f");assert(state.input.text==first);
  shell->onRawInput("\x1b[D\x1b[3~");assert(state.input.text.empty());
  state.input.pastedContents=Json::object();
  shell->onRawInput("h");shell->onRawInput("i");
  assert(state.input.text=="hi"&&state.input.caretUtf16==2);
  shell->onRawInput("\x1b[?1;2c");assert(replies==1);
  auto frame=shell->renderSnapshot();
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
    ftxui::Dimension::Fixed(12));
  ftxui::Render(screen,frame);shell->commitFrame();
  assert(screen.ToString().find("Batbox")!=std::string::npos);
  assert(screen.ToString().find("WELCOME_BAT")!=std::string::npos);
  shell->setRemembering(true);assert(shell->viewState().remembering);
  shell->setRemembering(false);assert(!shell->viewState().remembering);
  const auto paint=[&]{
    for(int pass=0;pass<4;++pass){
      screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
        ftxui::Dimension::Fixed(12));
      ftxui::Render(screen,shell->renderSnapshot());shell->commitFrame();
    }
  };
  UiRequest approval;
  approval.kind=UiRequest::Kind::Select;approval.title="Allow Bash?";
  approval.bodyPreviewLines=3;
  approval.choices={"Allow once","Always allow this rule","Deny","Cancel"};
  const std::string multiline="python3 - <<'EOF'\nimport json\nprint('preview')\n"+
    std::string(4000,'x')+"\nHIDDEN_COMMAND_TAIL";
  for(const auto& command:{multiline,std::string(10000,'x')+"HIDDEN_COMMAND_TAIL"}){
    approval.body=command;
    for(const auto [columns,rows]:{std::pair{40,12},std::pair{20,8},std::pair{20,4}}){
      state.modal=approval;updateViewport(state,columns,rows);
      auto canvas=ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
        ftxui::Dimension::Fixed(rows));
      ftxui::Render(canvas,shell->renderSnapshot());shell->commitFrame();
      const auto rendered=canvas.ToString();
      for(const auto* choice:{"Allow once","Always","Deny","Cancel"})
        assert(rendered.find(choice)!=std::string::npos);
      assert(rendered.find("HIDDEN_COMMAND_TAIL")==std::string::npos);
      if(rows>=8)assert(rendered.find("…")!=std::string::npos);
    }
  }
  // At less than four rows, keyboard navigation still exposes the selected action.
  approval.selectedIndex=3;state.modal=approval;updateViewport(state,20,3);
  auto tiny=ftxui::Screen::Create(ftxui::Dimension::Fixed(20),ftxui::Dimension::Fixed(3));
  ftxui::Render(tiny,shell->renderSnapshot());shell->commitFrame();
  assert(tiny.ToString().find("Cancel")!=std::string::npos);
  state.modal.reset();updateViewport(state,40,12);paint();
  state.streamingText="FIRST_REPLY";paint();
  assert(screen.ToString().find("WELCOME_BAT")!=std::string::npos);
  assert(screen.ToString().find("FIRST_REPLY")!=std::string::npos);
  assert(shell->viewport().ranges().keys().size()==1);
  assert(shell->viewport().ranges().origin()==1);
  state.streamingText.reset();paint();
  state.input.text="first line\nsecond line\nthird line\nfourth line\n"
    "fifth line\nsixth line\nlast VISIBLE_END";
  state.input.caretUtf16=value::stringLength(state.input.text);
  paint();
  assert(screen.ToString().find("VISIBLE_END")!=std::string::npos);
  assert(screen.ToString().find("first line")==std::string::npos);
  state.input.caretUtf16=0;paint();
  assert(screen.ToString().find("first line")!=std::string::npos);
  assert(screen.ToString().find("VISIBLE_END")==std::string::npos);
  state.input.text=std::string(85,'x')+" WRAPPED_END";
  state.input.caretUtf16=value::stringLength(state.input.text);paint();
  assert(screen.ToString().find("WRAPPED_END")!=std::string::npos);
  state.input.text="hi";state.input.caretUtf16=2;
  std::string tail;
  for(int line=0;line<80;++line)tail+="live row "+std::to_string(line)+" ";
  state.streamingText=tail+"BOTTOM_MARKER";paint();
  assert(screen.ToString().find("BOTTOM_MARKER")!=std::string::npos);
  assert(screen.ToString().find("WELCOME_BAT")==std::string::npos);
  shell->viewport().scrollTo(0);paint();
  assert(screen.ToString().find("WELCOME_BAT")!=std::string::npos);
  assert(shell->viewport().ranges().keys().size()==1);
  assert(shell->viewport().ranges().origin()==1);
  shell->jumpToBottom();paint();
  assert(screen.ToString().find("BOTTOM_MARKER")!=std::string::npos);
  showStartup=false;
  shell->scroll(-5);paint();
  assert(!shell->viewport().scroll().sticky&&shell->viewState().pillVisible);
  assert(screen.ToString().find("Go to the action")!=std::string::npos);
  const auto readerTop=shell->viewport().scroll().top;
  *state.streamingText+="\nNEW_OUTPUT_AT_BOTTOM";paint();
  assert(shell->viewport().scroll().top==readerTop);
  assert(screen.ToString().find("NEW_OUTPUT_AT_BOTTOM")==std::string::npos);
  shell->scroll(10000);paint();
  assert(shell->viewport().scroll().sticky&&!shell->viewState().pillVisible);
  assert(screen.ToString().find("NEW_OUTPUT_AT_BOTTOM")!=std::string::npos);
  shell->scroll(-4);paint();
  // SGR mouse coordinates are one-based; the button is above the input.
  shell->onRawInput("\x1b[<0;4;11M");paint();
  assert(shell->viewport().scroll().sticky);
  assert(screen.ToString().find("NEW_OUTPUT_AT_BOTTOM")!=std::string::npos);
  state.streamingText.reset();paint();
  shell->setTranscript(true);shell->openSearch();
  shell->onRawInput("n");
  assert(shell->navigation().searchQuery()=="n");
  shell->onRawInput("\x1b");
  loop.run();
  assert(!shell->viewState().searchOpen);
  UiRequest modal;modal.kind=UiRequest::Kind::Select;
  modal.id="choose";modal.title="Choose";modal.choices={"one","two"};
  shell->commands().start({"choose","",false,false},[&](
      CommandUi::Done done,CommandUi::Ready ready,CommandUi::Error){
    ready(modal,[done=std::move(done)](const UiAction& action){
      if(action.kind==UiAction::Kind::Activate){
        CommandCompletion completion;completion.result="selected";
        done(std::move(completion));
      }
    });
  });
  assert(state.modal&&!state.input.focused);
  shell->onRawInput("\x1b[B");
  assert(state.modal->selectedIndex==1);
  shell->onRawInput("\r");loop.runReady();
  assert(!state.modal&&state.input.focused&&settled==1);
  shell->stop();
  std::cout<<"native conversation shell workflow passed\n";
}
