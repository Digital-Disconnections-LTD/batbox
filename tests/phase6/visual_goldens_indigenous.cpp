// Golden frames for the Batbox visual identity across color depths. Each scene
// uses the production providers: presentation bindings, chrome, the
// permission provider and the default Gadget view builders.
#include "GoldenFrame.hpp"
#include "application_commands/ConnectorsCommand.hpp"
#include "application_commands/ProvidersCommand.hpp"
#include "application_gadgets/DefaultInputs.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "application_presentation/Bindings.hpp"
#include "interactive/conversation_shell/PoetryViewport.hpp"
#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "interactive/message_view/RetainedViews.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include <ftxui/screen/terminal.hpp>
#include <chrono>
#include <iostream>
#include <thread>
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
  std::string status;
  explicit Fixture(const std::string& name,std::string cwd="/home/bat/roost",
      std::string model="night-owl-7b",std::string notice={}):
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
    inputs.modelLabel=[model]{return model;};
    inputs.workingDirectory=[cwd]{return cwd;};
    inputs.homeDirectory=[]{return std::string("/home/bat");};
    inputs.permissionMode=[]{return std::string("plan");};
    inputs.statusText=[this]{return status;};
    inputs.gitBranch=[]{return std::string("night-shift");};
    inputs.contextUsage=[]()->std::optional<double>{return 0.42;};
    inputs.nowMs=[]{return 1234.0;};
    inputs.colors=name!="monochrome";
    if(!notice.empty())inputs.notice=[notice]{return notice;};
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
  inputs.workingDirectory=[]{return std::string("/home/bat/roost");};
  inputs.homeDirectory=[]{return std::string("/home/bat");};
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

// A small fixed catalog keeps these frames stable when the bundled one is
// regenerated from models.dev.
const char* providersCatalog=R"CAT({"defaults":{"context":131072,"output":16384},"providers":[
{"id":"opencode","name":"OpenCode Zen","kind":"anthropic","base_url":"https://opencode.ai/inference",
 "messages_path":"/anthropic/v1/messages","models_path":"/v1/models","key_env":["OPENCODE_API_KEY"],
 "auth":"key","auth_header":"authorization","models":[["claude-sonnet-5-5","Claude Sonnet 5.5",1000000,128000,"trip"]]},
{"id":"anthropic","name":"Anthropic","kind":"anthropic","base_url":"https://api.anthropic.com",
 "messages_path":"/v1/messages","key_env":["ANTHROPIC_API_KEY"],"auth":"key","auth_header":"x-api-key","models":[]},
{"id":"openrouter","name":"OpenRouter","kind":"anthropic","base_url":"https://openrouter.ai",
 "messages_path":"/api/v1/messages","models_path":"/api/v1/models","key_env":["OPENROUTER_API_KEY"],
 "auth":"key","auth_header":"authorization","doc":"https://openrouter.ai/keys","models":[
 ["qwen/qwen3-coder","Qwen3 Coder",262144,65536,"t"],["anthropic/claude-sonnet-5.5","Claude Sonnet 5.5",1000000,128000,"trip"],
 ["deepseek/deepseek-v4-pro","DeepSeek V4 Pro",1048576,393216,"tr"]]},
{"id":"github-copilot","name":"GitHub Copilot","kind":"anthropic","base_url":"https://api.githubcopilot.com",
 "messages_path":"/v1/messages","auth":"oauth-device","models":[]},
{"id":"groq","name":"Groq","kind":"openai-chat","base_url":"https://api.groq.com/openai",
 "messages_path":"/v1/chat/completions","key_env":["GROQ_API_KEY"],"auth":"key","models":[]},
{"id":"ollama","name":"Ollama","kind":"anthropic","base_url":"http://127.0.0.1:11434","messages_path":"/v1/messages",
 "auth":"none","auth_header":"none","local":true,"models":[]},
{"id":"llama.cpp","name":"llama.cpp (llama-server)","kind":"anthropic","base_url":"http://127.0.0.1:9931",
 "messages_path":"/v1/messages","auth":"none","auth_header":"none","local":true,"models":[]},
{"id":"perplexity","name":"Perplexity","kind":"unsupported","reason":"Uses its own protocol; not supported yet.",
 "auth":"key","models":[]}]})CAT";

struct ProvidersScenes {golden::Frame list,key,models,saved;std::string notice;};
ProvidersScenes providersScenes(Fixture& fixture){
  static const auto catalog=providers::parseCatalog(Json::parse(providersCatalog));
  application_commands::ProvidersInputs inputs;
  inputs.catalog=&catalog;inputs.animate=false;
  inputs.environment=[](const std::string& name)->std::optional<std::string>{
    if(name=="GROQ_API_KEY")return std::string("set");
    return std::nullopt;};
  inputs.activeProfile=[]{return std::string("home-vllm::qwen3-coder");};
  inputs.discover=[]{
    providers::LocalServer ollama;
    ollama.preset="ollama";ollama.name="Ollama";ollama.baseUrl="http://127.0.0.1:11434";
    ollama.port=11434;ollama.models={"qwen3-coder:30b","gemma4:31b","llama3.3:70b"};
    return std::vector<providers::LocalServer>{ollama};
  };
  inputs.readEndpoints=[]{
    application::EndpointRecord home;
    home.label="Home vLLM";home.baseUrl="http://10.0.0.5:8000";
    home.models={"qwen3-coder"};home.contextWindow=65536;home.maxOutputTokens=8192;
    return std::map<std::string,application::EndpointRecord>{{"home-vllm",home}};
  };
  inputs.test=[](const application::EndpointRecord&){
    providers::ConnectionResult result;
    result.reached=result.ok=true;result.status=200;
    result.models={"qwen/qwen3-coder","anthropic/claude-sonnet-5.5","deepseek/deepseek-v4-pro",
      "mistralai/devstral-2"};
    result.message="Connected to openrouter.ai · 4 models";
    return result;
  };
  std::optional<std::string> savedProfile;
  inputs.save=[&savedProfile](std::map<std::string,application::EndpointRecord> endpoints,
      const std::string& id,application::EndpointRecord endpoint,const std::string& model){
    check(endpoint.provider=="anthropic"&&endpoint.preset=="openrouter"&&
      endpoint.authToken=="sk-or-v1-golden"&&endpoint.contextWindow==262144&&
      endpoint.maxOutputTokens==65536,"saved OpenRouter record");
    providers::SaveResult result;
    result.endpointId=id;result.profile=id+"::"+model;result.endpoint=endpoint;
    endpoints[id]=endpoint;result.endpoints=std::move(endpoints);
    savedProfile=result.profile;
    return result;
  };
  std::optional<UiRequest> shown;
  interactive::CommandUi::Action act;
  std::optional<std::string> finished;
  application_commands::startProvidersDialog(fixture.loop,
    application_commands::completeProvidersInputs(std::move(inputs)),
    [&](interactive::CommandCompletion done){finished=done.result.value_or("");},
    [&](std::optional<UiRequest> request,interactive::CommandUi::Action action){
      shown=std::move(request);act=std::move(action);});
  const auto settle=[&](const std::function<bool()>& ready,const std::string& what){
    for(int i=0;i<400&&!ready();++i){
      fixture.loop.runReady();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(ready(),"providers dialog never reached "+what);
  };
  const auto frame=[&]{
    fixture.state.modal=*shown;
    auto view=fixture.view(80,24);
    auto result=golden::render(fixture.shell.chrome.renderModal(view),80,22);
    fixture.state.modal.reset();
    return result;
  };
  const auto type=[&](const std::string& text){
    act({UiAction::Kind::TextChange,0,text});
  };
  settle([&]{return shown&&shown->body.find("Looking")==std::string::npos&&
    !shown->choices.empty()&&shown->choices.front()=="Ollama";},"discovered servers");
  ProvidersScenes scenes;
  scenes.list=frame();
  type("router");
  check(shown->choices.size()==1&&shown->choices.front()=="OpenRouter","search for OpenRouter");
  act({UiAction::Kind::Activate,0,{}});
  check(shown->id=="providers-key-input"&&shown->sensitiveInput,"masked key field");
  type("sk-or-v1-golden");
  scenes.key=frame();
  check(scenes.key.text.find("sk-or")==std::string::npos,"the key must stay masked");
  act({UiAction::Kind::Activate,0,{}});
  check(shown->id=="providers-base-url"&&shown->body.find("https://openrouter.ai▌")!=std::string::npos,
    "base URL prefilled");
  act({UiAction::Kind::Activate,0,{}});
  settle([&]{return shown->id=="providers-models";},"the model step");
  scenes.models=frame();
  act({UiAction::Kind::Activate,0,{}});
  settle([&]{return finished.has_value();},"the saved notice");
  scenes.notice=*finished;
  check(savedProfile=="openrouter::qwen/qwen3-coder","saved profile");
  fixture.status=scenes.notice;
  scenes.saved=golden::render(fixture.shell.chrome.renderStatus(fixture.view(100,24)),100,1);
  fixture.status.clear();
  return scenes;
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
      check(contains(permission,"│ src/lexer.cpp")&&
        permission.text.find("│ /home/bat/roost/src/lexer.cpp")==std::string::npos,
        theme+": permission dialog must name the file relative to the Roost");
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
      const auto providers=providersScenes(fixture);
      check(contains(providers.list,"Found on this machine")&&contains(providers.list,"Ollama")&&
        contains(providers.list,"3 models")&&contains(providers.list,"Configured")&&
        contains(providers.list,"active")&&contains(providers.list,"Cloud")&&
        contains(providers.list,"key found"),theme+": provider list groups");
      check(contains(providers.key,"OpenRouter  ·  API key")&&
        contains(providers.key,"***************"),theme+": masked key step");
      check(contains(providers.models,"qwen/qwen3-coder")&&contains(providers.models,"256K context · 64K output")&&
        contains(providers.models,"tools · reasoning")&&contains(providers.models,"mistralai/devstral-2"),
        theme+": model step merges the live list with catalog limits");
      check(providers.notice=="Added openrouter · qwen/qwen3-coder · /model to switch",
        theme+": saved notice "+providers.notice);
      check(contains(providers.saved,"Added openrouter"),theme+": saved notice in the status row");
      for(const auto& [scene,frame]:std::vector<std::pair<std::string,golden::Frame>>{
          {"welcome",welcome},{"transcript",transcript},{"gadgets",gadgets},
          {"permission",permission},{"input",input},{"providers-list",providers.list},
          {"providers-key",providers.key},{"providers-models",providers.models},
          {"providers-saved",providers.saved}})
        passed=golden::compare(directory,scene+"-"+theme,frame)&&passed;
    }
    {
      // A long Roost outside home keeps its last two directories whole.
      Fixture deep("batbox-dark","/Volumes/Night/projects/2026/BATBOX/batbox",
        "anthropic::claude-haiku-5-5");
      const auto welcome=welcomeScene(deep);
      check(contains(welcome,"…/BATBOX/batbox"),
        "welcome card must keep the Roost's last two directories");
      check(welcome.text.find("…/BATBO…")==std::string::npos&&
        welcome.text.find("…/batbox ")==std::string::npos,
        "welcome card must not cut the Roost path mid-name:\n"+welcome.text);
    }
    {
      // A Bat view keeps a transient notice apart from its follow-up hints.
      Fixture bat("batbox-dark");
      bat.status="Ctrl+B to run in background";
      auto view=bat.view(120,24);
      view.batId="bat-1";view.batLabel="test-audit";view.batStatus="running";
      const auto row=golden::render(bat.shell.chrome.renderStatus(view),120,1);
      check(contains(row,"Ctrl+B to run in background · Follow-up → Bat"),
        "Bat status row must separate the notice from its hints");
    }
    {
      // Missing web helpers add one line to the welcome card; /connectors
      // lists each helper with its state and fix.
      Fixture web("batbox-dark","/home/bat/roost","night-owl-7b",
        "Web search and fetch are not set up · run: batbox setup web");
      const auto welcome=welcomeScene(web);
      check(contains(welcome,"Web search and fetch are not set up · run: batbox setup web")&&
        contains(welcome,"night-owl-7b"),"welcome card must show the web helper notice");
      namespace ac=application_connectors;
      const std::vector<ac::HelperCheck> helpers{
        {"Scrapling","missing","\"scrapling\" not found",
          "batbox setup web, or set scrapling_command to the scrapling executable",false},
        {"SearXNG","reachable",
          "http://127.0.0.1:8888/search (~/.batbox/searxng/settings.yml) · JSON on · 12 results",
          "",true}};
      const std::vector<ac::HelperCheck> servers{
        ac::mcpServerCheck("docs","connected",""),
        ac::mcpServerCheck("tickets","failed","Connection closed")};
      web.state.modal=application_commands::connectorsRequest(helpers,servers);
      auto view=web.view(80,24);
      const auto dialog=golden::render(web.shell.chrome.renderModal(view),80,22);
      web.state.modal.reset();
      check(contains(dialog,"Connectors")&&contains(dialog,"Web helpers")&&
        contains(dialog,"Scrapling")&&contains(dialog,"missing")&&
        contains(dialog,"MCP servers")&&contains(dialog,"tickets")&&
        contains(dialog,"Set up now"),"connectors dialog lacks helpers, servers or setup");
      passed=golden::compare(directory,"connectors-batbox-dark",dialog)&&passed;
    }
    {
      // The model names its provider where there is room and gives the
      // prefix up first when the row is tight.
      Fixture routed("batbox-dark","/home/bat/roost","openrouter · qwen/qwen3-coder");
      const auto welcome=welcomeScene(routed);
      check(contains(welcome,"openrouter · qwen/qwen3-coder"),
        "welcome card must name the provider beside the model:\n"+welcome.text);
      const auto wide=golden::render(routed.shell.chrome.renderStatus(routed.view(120,24)),120,1);
      check(contains(wide,"openrouter · qwen/qwen3-coder")&&contains(wide,"~/roost"),
        "wide status row must name the provider:\n"+wide.text);
      const auto narrow=golden::render(routed.shell.chrome.renderStatus(routed.view(56,24)),56,1);
      check(contains(narrow,"qwen/qwen3-coder")&&!contains(narrow,"openrouter"),
        "narrow status row must drop the provider before the model:\n"+narrow.text);
    }
    if(!passed)return 1;
    std::cout<<"Batbox visual goldens match across four themes\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
