// Golden frames for the Batbox visual identity across color depths. Each scene
// uses the production providers: presentation bindings, chrome, the
// permission provider and the default Gadget view builders.
#include "GoldenFrame.hpp"
#include "application_gadgets/DefaultInputs.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "application_presentation/Bindings.hpp"
#include "interactive/conversation_shell/PoetryViewport.hpp"
#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "interactive/message_view/RetainedViews.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include <ftxui/screen/terminal.hpp>
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
namespace ap=batbox::application_presentation;
namespace ip=batbox::interactive::presentation_providers;
namespace mv=batbox::interactive::message_view;
namespace gv=batbox::interactive::gadget_views;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
bool contains(const golden::Frame& frame,const std::string& text){
  const bool found=frame.text.find(text)!=std::string::npos;
  if(!found)std::cerr<<"Missing \""<<text<<"\" in frame:\n"<<frame.text<<"\n";
  return found;
}

const char* markdown=R"(## Parser notes

The lexer now keeps `Span` offsets; see [the design](https://example.com/spans).

- Tokens carry their source range, so diagnostics can point at the exact column instead of the line.
  - Nested items get their own marker.
- [x] Keep the old API
- [ ] Remove the shim

```cpp
// Tokens remember where they came from.
struct Token { int start = 0; std::string text; };
```

| Stage | Time |
|-------|-----:|
| lex | 4 ms |

---

Done.)";

struct Fixture {
  event_loop::Loop loop;
  PresentationState state;
  terminal_ftxui::Options options;
  conversation_shell::Imports shell;
  ip::ThemeState theme;
  messages::Services messages;
  gadgets::Gadget read,edit,bash;
  gadgets::Gadgets active;
  std::shared_ptr<ap::Bundle> bundle;
  explicit Fixture(const std::string& name):
      shell(loop,state,(options.interactive=false,options)),
      theme({[name]{return name;},[](const std::string&){},
        []{return std::string("dark");},[]{}},name){
    state.viewport.columns=80;state.viewport.rows=40;
    messages.strictGadgetResultPairing=[]{return false;};
    messages.featureGate=[](const std::string&){return false;};
    messages.logError=[](const std::string&){};
    const auto file=[](gadgets::Gadget& gadget,const std::string& gadgetName){
      gadget.name=gadgetName;
      gadget.parseInput=[](const Json& input){return input;};
      gadget.userFacingName=[gadgetName](const Json&){return gadgetName;};
      gadget.renderGadgetUseMessage=[gadgetName](const Json& input,const Json&){
        return application_gadgets::fileGadgetUseView(gadgetName,input,
          "/home/bat/roost","/home/bat");};
      gadget.renderGadgetResultMessage=[gadgetName](const Json& output,const Json&,
          const Json& options){
        return application_gadgets::fileGadgetResultView(gadgetName,output,options,
          "/home/bat/roost","/home/bat");};
    };
    file(read,"Read");file(edit,"Edit");
    bash.name="Bash";
    bash.parseInput=[](const Json& input){return input;};
    bash.userFacingName=[](const Json&){return std::string("Bash");};
    bash.renderGadgetUseMessage=[](const Json& input,const Json&){
      return Json("$ "+input.value("command",std::string{}));};
    bash.renderGadgetResultMessage=[](const Json& output,const Json&,const Json& options){
      return application_gadgets::shellGadgetResultView(output,options);};
    active={&read,&edit,&bash};
    ap::Inputs inputs;
    inputs.theme=&theme;inputs.messageServices=&messages;
    inputs.shortcuts.loadBindings=[]{return std::vector<ip::Binding>{};};
    inputs.gadgets=[this]{return active;};inputs.commands=[]{return Json::array();};
    inputs.modelLabel=[]{return std::string("night-owl-7b");};
    inputs.workingDirectory=[]{return std::string("/home/bat/roost");};
    inputs.homeDirectory=[]{return std::string("/home/bat");};
    inputs.permissionMode=[]{return std::string("plan");};
    inputs.statusText=[]{return std::string{};};
    inputs.gitBranch=[]{return std::string("night-shift");};
    inputs.contextUsage=[]()->std::optional<double>{return 0.42;};
    inputs.nowMs=[]{return 1234.0;};
    inputs.colors=name!="monochrome";
    bundle=ap::Bundle::bind(shell,std::move(inputs));
  }
  conversation_shell::ViewState view(int columns,int rows){
    static transcript_navigation::StickyPoetry sticky;
    static std::optional<message_actions::Cursor> cursor;
    conversation_shell::ViewState result{state,sticky,cursor,{},"",0,0,0,columns,rows,
      false,false,false};
    result.welcomeFrame=conversation_shell::welcomeAnimationFrames;
    return result;
  }
};

golden::Frame welcomeScene(Fixture& fixture){
  auto view=fixture.view(80,30);
  view.bodyRows=27;view.bodyScreenRow=1;
  return golden::render(fixture.shell.chrome.renderWelcome(view),80,27);
}

golden::Frame transcriptScene(Fixture& fixture){
  auto user=std::make_shared<Json>(Json{{"type","user"},{"uuid","u"},
    {"message",{{"content",Json::array({{{"type","text"},
      {"text","Teach the lexer about source spans"}}})}}}});
  auto assistant=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a"},
    {"message",{{"content",Json::array({{{"type","text"},{"text",markdown}}})}}}});
  auto render=fixture.shell.projectionServices();
  render.lookups=std::make_shared<MessageLookups>();
  mv::MessageOptions options;options.columns=80;options.addMargin=true;
  auto imports=fixture.shell.messageImports();
  ftxui::Elements rows;
  for(const auto& message:{user,assistant})
    if(auto element=mv::projectMessage(mv::prepareMessage(RenderItem(message),options,
        imports),render))rows.push_back(*element);
  return golden::render(ftxui::vbox(std::move(rows)),80,40);
}

golden::Frame gadgetScene(Fixture& fixture){
  const auto services=fixture.shell.projectionServices();
  const auto& use=services.gadgetUseProjection;
  const auto retained=fixture.shell.liveRetainedViews();
  const auto& result=retained.result.successProjection.renderGadgetMessage;
  const auto header=[&](const std::string& name,const Json& text,bool resolved,bool error){
    cards_gadget_use::GadgetUseCard card;
    card.visible=true;card.name=name;card.useText=text;card.showDot=true;
    card.resolved=resolved;card.error=error;
    card.nameWidth=static_cast<int>(name.size())+2;
    if(!resolved)card.progressKind=cards_gadget_use::GadgetUseCard::ProgressKind::GadgetProgress;
    return *cards_gadget_use::projectGadgetUse(card,use);
  };
  const Json noOptions=Json::object();
  ftxui::Elements rows;
  rows.push_back(header("Read",application_gadgets::fileGadgetUseView("Read",
    {{"file_path","/home/bat/roost/src/lexer.cpp"}},"/home/bat/roost","/home/bat"),true,false));
  std::string content;
  for(int i=1;i<=20;++i)content+="line "+std::to_string(i)+"\tof the lexer\n";
  rows.push_back(result(application_gadgets::fileGadgetResultView("Read",
    {{"type","text"},{"file",{{"filePath","/home/bat/roost/src/lexer.cpp"},
      {"content",content},{"numLines",20},{"startLine",1},{"totalLines",20}}}},
    Json{{"verbose",true}},"/home/bat/roost","/home/bat")));
  rows.push_back(ftxui::text(""));
  rows.push_back(header("Edit",application_gadgets::fileGadgetUseView("Edit",
    {{"file_path","/home/bat/roost/src/lexer.cpp"}},"/home/bat/roost","/home/bat"),true,false));
  const Json patch=Json::array({{{"oldStart",11},{"newStart",11},{"lines",Json::array({
    " Token next() {","-  return {pos};","+  // Keep the span.","+  return {pos, span()};"," }"})}}});
  rows.push_back(result(application_gadgets::fileGadgetResultView("Edit",
    {{"filePath","/home/bat/roost/src/lexer.cpp"},{"structuredPatch",patch}},
    noOptions,"/home/bat/roost","/home/bat")));
  rows.push_back(ftxui::text(""));
  rows.push_back(header("Bash",Json("$ ctest --output-on-failure"),true,false));
  std::string out="\x1b[32mok\x1b[0m\tlexer\n";
  for(int i=1;i<=29;++i)out+="test "+std::to_string(i)+" passed\n";
  rows.push_back(result(application_gadgets::shellGadgetResultView(
    {{"stdout",out},{"stderr","warning: 1 test was slow"},{"interrupted",false}},noOptions)));
  rows.push_back(ftxui::text(""));
  rows.push_back(header("Bash",Json("$ make lint"),false,false));
  return golden::render(ftxui::vbox(std::move(rows)),80,50);
}

golden::Frame permissionScene(Fixture& fixture){
  std::optional<UiRequest> shown;
  auto ui=std::make_shared<CommandUi>(fixture.loop,CommandUi::Callbacks{
    [&](std::optional<UiRequest> request){if(request)shown=request;},
    [](const CommandDescriptor&,const CommandCompletion&,bool){},
    [](const std::string&){},[](bool){},[](bool){}});
  application_permissions::PermissionProvider::Inputs inputs(fixture.loop);
  inputs.ui=ui;
  inputs.persistAllowRule=[](const std::string&){};
  auto provider=application_permissions::PermissionProvider::create(std::move(inputs));
  auto decision=provider->check("Edit",{{"file_path","/home/bat/roost/src/lexer.cpp"},
    {"old_string","  return {pos};\n"},{"new_string","  // Keep the span.\n  return {pos, span()};\n"}},
    Json{{"mode","default"}});
  for(int i=0;i<20&&!shown;++i)fixture.loop.runReady();
  check(shown.has_value(),"Permission request did not reach the dialog");
  fixture.state.modal=*shown;
  auto view=fixture.view(80,24);
  auto frame=golden::render(fixture.shell.chrome.renderModal(view),80,22);
  fixture.state.modal.reset();
  provider->cancelAll();
  for(int i=0;i<20;++i)fixture.loop.runReady();
  return frame;
}

golden::Frame inputScene(Fixture& fixture){
  const auto frameInput=[&](const conversation_shell::ViewState& view){
    const auto decor=fixture.shell.chrome.poetryDecor(view);
    auto poetry=conversation_shell::poetryViewport(fixture.state.input,
      view.columns-conversation_shell::poetryFrameInset,4,fixture.shell.editorMetrics,
      true,&decor);
    return ftxui::vbox({ftxui::hbox({ftxui::text(" "),std::move(poetry)|ftxui::xflex,
        ftxui::text(" ")})|ftxui::borderStyled(ftxui::ROUNDED,decor.border),
      fixture.shell.chrome.renderStatus(view)});
  };
  ftxui::Elements rows;
  fixture.state.input.text.clear();
  rows.push_back(frameInput(fixture.view(80,24)));
  fixture.state.input.text="Teach the lexer about source spans";
  fixture.state.input.caretUtf16=fixture.state.input.text.size();
  fixture.state.loading=true;
  auto working=fixture.view(80,24);
  working.workingMs=4200;working.streamMode="thinking";working.activityFrame=3;
  rows.push_back(frameInput(working));
  fixture.state.loading=false;
  return golden::render(ftxui::vbox(std::move(rows)),80,8);
}
} // namespace

int main(int argc,char** argv){
  try{
    check(argc==2,"Golden directory required");
    const std::filesystem::path directory=argv[1];
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
    ip::applyColorDepth(ip::ColorDepth::TrueColor);
    bool passed=true;
    for(const std::string theme:{"batbox-dark","batbox-light","batbox-ansi","monochrome"}){
      Fixture fixture(theme);
      const auto& roles=ip::styleRoles(theme);
      const auto welcome=welcomeScene(fixture);
      check(contains(welcome,"(o.o)")&&contains(welcome,"(o o)")&&
        contains(welcome,"Batbox")&&contains(welcome,"code after dark"),
        theme+": welcome lost its companions");
      check(contains(welcome,"night-owl-7b")&&contains(welcome,"~/roost")&&
        contains(welcome,"◆ plan")&&contains(welcome,"/help"),
        theme+": welcome card lacks model, Roost, mode or tips");
      const auto transcript=transcriptScene(fixture);
      check(contains(transcript,"❯ Teach the lexer"),theme+": user turn lacks its prompt glyph");
      check(contains(transcript,"◆ Parser notes"),theme+": heading glyph missing");
      check(contains(transcript,"╭─ cpp"),theme+": code block frame missing");
      check(contains(transcript,"[✓] Keep the old API")&&contains(transcript,"[ ] Remove"),
        theme+": task list state missing");
      check(contains(transcript,"• Tokens carry")&&contains(transcript,"◦ Nested"),
        theme+": list markers missing");
      check(transcript.text.find("```")==std::string::npos,theme+": raw fence leaked");
      const auto gadgets=gadgetScene(fixture);
      check(contains(gadgets,"✓ Read src/lexer.cpp")&&contains(gadgets,"✓ Edit src/lexer.cpp"),
        theme+": Gadget headers must name the target once, relative to the Roost");
      check(contains(gadgets,"⎿  Updated src/lexer.cpp with 2 additions and 1 removal"),
        theme+": Edit summary missing");
      check(contains(gadgets,"12 - ")&&contains(gadgets,"12 + ")&&contains(gadgets,"13 + "),
        theme+": diff line numbers missing");
      check(contains(gadgets,"ok      lexer")&&gadgets.text.find("\x1b")==std::string::npos,
        theme+": shell output not sanitized");
      check(contains(gadgets,"… +19 lines (ctrl+o to expand)"),
        theme+": long shell output not truncated with an expand hint");
      check(contains(gadgets,"⠹ Bash $ make lint"),theme+": running Gadget lacks its spinner");
      const auto permission=permissionScene(fixture);
      check(contains(permission,"Allow Edit?")&&contains(permission,"Allow once")&&
        contains(permission,"Saves Edit(/home/bat/roost/src/lexer.cpp)"),
        theme+": permission dialog must name the rule it saves");
      check(contains(permission,"-   return {pos};")&&contains(permission,"+   // Keep the span."),
        theme+": permission dialog must preview the edit");
      const auto input=inputScene(fixture);
      check(contains(input,"❯ Give your model a destination… (/help)"),
        theme+": empty input lacks its placeholder");
      check(contains(input,"◆ plan")&&contains(input,"night-owl-7b")&&
        contains(input,"~/roost")&&contains(input,"⎇ night-shift")&&
        contains(input,"42% context"),theme+": status row lacks context");
      check(contains(input,"Thinking…")&&contains(input,"4s")&&contains(input,"to interrupt"),
        theme+": working status lacks verb, elapsed time or interrupt hint");
      if(!roles.monochrome){
        check(gadgets.styles.find("fg="+roles.diffAdd.Print(false))!=std::string::npos,
          theme+": added diff lines lost their color");
        check(transcript.styles.find("fg="+roles.userBar.Print(false))!=std::string::npos,
          theme+": user prompt glyph lost its role color");
      }else{
        check(gadgets.styles.find("fg=3")==std::string::npos&&
          transcript.styles.find("fg=3")==std::string::npos,
          "monochrome painted an explicit color");
      }
      for(const auto& [scene,frame]:std::vector<std::pair<std::string,golden::Frame>>{
          {"welcome",welcome},{"transcript",transcript},{"gadgets",gadgets},
          {"permission",permission},{"input",input}})
        passed=golden::compare(directory,scene+"-"+theme,frame)&&passed;
    }
    if(!passed)return 1;
    std::cout<<"Batbox visual goldens match across four themes\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
