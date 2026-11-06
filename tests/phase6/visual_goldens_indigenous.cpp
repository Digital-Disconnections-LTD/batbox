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
#include "interactive/presentation_providers/MissionFooter.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "interactive/presentation_providers/CardPalettes.hpp"
#include "terminal_ftxui/PresentationView.hpp"
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
    conversation_shell::ViewState result{state,sticky,{},"",0,0,0,columns,rows,
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
// Three Bats, one launched by another, and a background shell. The fixture
// clock reads 1234 ms, so elapsed times stay fixed.
Json missionFixture(){
  const auto bat=[](const std::string& description,long long start,long long gadgets,
      const std::string& activity,const std::string& said){
    return Json{{"type","local_agent"},{"agentType","general-purpose"},{"status","running"},
      {"description",description},{"startTime",start},{"isBackgrounded",true},
      {"progress",{{"toolUseCount",gadgets},{"lastText",said},
        {"lastActivity",{{"toolName","Read"},{"activityDescription",activity}}}}}};
  };
  Json missions={
    {"bat-audit",bat("Audit the parser",3,4,"Reading src/parser.cpp","Two spans drift")},
    {"bat-lexer",bat("Check the lexer",30,2,"Searching for Span","Offsets look right")},
    {"bat-notes",bat("Draft release notes",2,1,"Reading CHANGELOG.md","")},
    {"shell-watch",{{"type","local_bash"},{"status","running"},{"description","npm test --watch"},
      {"isBackgrounded",true},{"startTime",1}}}};
  missions["bat-lexer"]["parentBatId"]="bat-audit";
  return missions;
}
struct MissionScenes {golden::Frame expanded,collapsed,breadcrumb,home;};
MissionScenes missionScenes(Fixture& fixture){
  const auto missions=missionFixture();
  const auto& chrome=fixture.shell.chrome;
  auto view=fixture.view(100,24);
  view.batId="bat-lexer";view.batLabel="Check the lexer";view.batStatus="running";
  view.backgroundMissions=ip::backgroundFooterMissions(missions,view.batId);
  view.batTrail=ip::batTrail(view.backgroundMissions,*view.batId);
  const auto bottom=[&](const conversation_shell::ViewState& state,int rows){
    return golden::render(ftxui::vbox({chrome.renderMissions(state),chrome.renderStatus(state)}),
      state.columns,rows);
  };
  MissionScenes scenes;
  scenes.expanded=bottom(view,6);
  scenes.breadcrumb=golden::render(chrome.renderStatus(view),100,1);
  view.missionsCollapsed=true;
  scenes.collapsed=bottom(view,2);
  auto home=fixture.view(120,24);
  home.backgroundMissions=ip::backgroundFooterMissions(missions);
  scenes.home=bottom(home,6);
  return scenes;
}
// Shell output wider than the terminal is cut by column: wide characters and
// color escapes stay whole on both sides of the cut.
golden::Frame shellOutputScene(Fixture& fixture){
  const std::vector<std::string> outputs={
    "make: building the lexer, the parser and the span tables",
    "\u6f22\u5b57\u306e\u30c6\u30b9\u30c8\u51fa\u529b\u3092\u5207\u308b\u6642"
      "\u3082\u6587\u5b57\u3092\u58ca\u3055\u306a\u3044\u3053\u3068\u3092"
      "\u78ba\u304b\u3081\u308b",
    "\x1b[31mFAILED lexer spans: expected 12 got 13 at column 40\x1b[39m done"};
  auto render=fixture.shell.projectionServices();
  render.lookups=std::make_shared<MessageLookups>();
  mv::MessageOptions options;options.columns=40;options.addMargin=false;
  const auto imports=fixture.shell.messageImports();
  ftxui::Elements rows;
  for(std::size_t i=0;i<outputs.size();++i){
    auto message=std::make_shared<Json>(Json{{"type","user"},{"uuid","shell-"+std::to_string(i)},
      {"message",{{"content",Json::array({{{"type","text"},{"text",
        "<bash-stdout>"+outputs[i]+"</bash-stdout><bash-stderr></bash-stderr>"}}})}}}});
    if(auto element=mv::projectMessage(mv::prepareMessage(RenderItem(message),options,
        imports),render))rows.push_back(*element);
  }
  return golden::render(ftxui::vbox(std::move(rows)),40,9);
}
// Poetry delivered at a Gadget round arrives as an attachment; it is drawn as
// the user's message.
golden::Frame deliveredScene(Fixture& fixture){
  auto delivered=std::make_shared<Json>(Json{{"type","attachment"},{"uuid","d"},
    {"attachment",{{"type","queued_command"},{"prompt","Also run the lexer tests"},
      {"source_uuid","q1"},{"commandMode","prompt"}}}});
  auto render=fixture.shell.projectionServices();
  render.lookups=std::make_shared<MessageLookups>();
  mv::MessageOptions options;options.columns=80;options.addMargin=true;
  ftxui::Elements rows;
  if(auto element=mv::projectMessage(mv::prepareMessage(RenderItem(delivered),options,
      fixture.shell.messageImports()),render))rows.push_back(*element);
  return golden::render(ftxui::vbox(std::move(rows)),80,3);
}
// Poetry sent while a turn runs: three queued kinds, a long message cut to
// three lines, one being delivered, and the status row's edit hint.
const std::vector<QueuedInput> queuedFixture{
  {"q1","Also run the lexer tests before you finish",QueuedInput::Kind::Poetry,false},
  {"q2","/compact",QueuedInput::Kind::Command,false},
  {"q3","make test",QueuedInput::Kind::Shell,false},
  {"q4","Then explain the span change\nwith the old offsets\nand the new ones\n"
    "and why the parser owns recovery",QueuedInput::Kind::Poetry,false},
  {"q5","Taken for delivery right now",QueuedInput::Kind::Poetry,true}};
golden::Frame queuedScene(Fixture& fixture){
  ftxui::Elements rows;
  for(std::size_t i=0;i<queuedFixture.size();++i)
    rows.push_back(fixture.shell.renderQueuedInput(queuedFixture[i],i==0,120));
  fixture.state.loading=true;fixture.state.queuedInputs=queuedFixture;
  auto view=fixture.view(120,24);
  view.workingMs=27*60000+13000;view.streamMode="tool-use";view.activityFrame=3;
  rows.push_back(fixture.shell.chrome.renderStatus(view));
  fixture.state.loading=false;fixture.state.queuedInputs.clear();
  return golden::render(ftxui::vbox(std::move(rows)),120,12);
}
// The same queue in --inline mode, under a short transcript.
golden::Frame queuedInlineScene(Fixture& fixture){
  terminal_ftxui::setInlinePalette(ip::inlinePalette(fixture.theme));
  PresentationState state;
  state.transcriptRows={
    {TranscriptRow::Kind::User,"u","Teach the lexer about source spans",false,"1:0"},
    {TranscriptRow::Kind::Assistant,"a","Running the tests now.",false,"2:0"}};
  state.queuedInputs=queuedFixture;
  state.loading=true;state.streamMode="tool-use";
  auto frame=golden::render(terminal_ftxui::renderPresentation(state,{100,14}),100,14);
  terminal_ftxui::setInlinePalette({});
  return frame;
}
// Two running Bats as a grouped Gadget row, and the one line it keeps while
// the Active Missions list shows them.
struct BatScenes {golden::Frame running,launched;};
BatScenes batScenes(Fixture& fixture){
  const auto services=fixture.shell.projectionServices();
  const Json group={{gv::viewKey,"group"},{"title","2 Bats"},{"running",true},
    {"entries",Json::array({"Research alpha · Latest tool: Read · 4s",
      "Research beta · Launched"})}};
  const auto& render=services.gadgetUseProjection.renderGadgetMessage;
  return {golden::render(render(group),80,4),
    golden::render(render(gv::launchedGroupView(group)),80,2)};
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
      const auto shellOutput=shellOutputScene(fixture);
      check(contains(shellOutput,"make: building the lexer, the")&&
        contains(shellOutput,"parser and the span tables"),
        theme+": plain shell output wraps at the column limit");
      check(shellOutput.text.find("\xef\xbf\xbd")==std::string::npos&&
        contains(shellOutput,"\u6f22\u5b57\u306e\u30c6\u30b9\u30c8")&&
        contains(shellOutput,"\u78ba\u304b\u3081\u308b"),
        theme+": a cut through wide characters must not split one");
      check(shellOutput.text.find("[31m")==std::string::npos&&
        shellOutput.text.find("[39m")==std::string::npos&&
        contains(shellOutput,"FAILED lexer spans: expected 1")&&
        contains(shellOutput,"2 got 13 at column 40 done"),
        theme+": a cut through colored output must keep its escapes whole");
      check(shellOutput.styles.find("4:5-34 fg=31 ")!=std::string::npos&&
        shellOutput.styles.find("5:5-25 fg=31 ")!=std::string::npos,
        theme+": the color must carry across the cut and end where the output resets it");
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
      const auto missions=missionScenes(fixture);
      check(contains(missions.expanded,"Active Missions (4) · ↓ select · Ctrl+T collapse · /missions")&&
        contains(missions.expanded,"  ● Audit the parser")&&
        contains(missions.expanded,"running  1s  4 Gadgets")&&
        contains(missions.expanded,"▸ └ ● Check the lexer")&&
        contains(missions.expanded,"  ● npm test --watch"),
        theme+": expanded Missions list lacks its tree or highlight");
      check(contains(missions.expanded,
          "Flight › Audit the parser › Check the lexer · Alt+↑↓ move · Alt+← home · Ctrl+T list"),
        theme+": Bat breadcrumb missing under the list");
      check(contains(missions.collapsed,"Active Missions (4) · in: Check the lexer")&&
        missions.collapsed.text.find("● Audit the parser")==std::string::npos,
        theme+": collapsed list must be one line naming the open Bat");
      check(contains(missions.breadcrumb,"Flight › Audit the parser › Check the lexer"),
        theme+": breadcrumb banner");
      check(contains(missions.home,"3 Bats running · ↓ Bats · Ctrl+T list")&&
        missions.home.text.find("▸")==std::string::npos&&
        missions.home.text.find("Flight ›")==std::string::npos,
        theme+": home marks no row and names the way into the Bats");
      const auto queued=queuedScene(fixture);
      check(contains(queued,"❯ Also run the lexer tests before you finish")&&
        contains(queued,"queued · delivered at the next Gadget round"),
        theme+": queued poetry lacks its text or label");
      check(contains(queued,"❯ /compact")&&contains(queued,"! make test")&&
        contains(queued,"queued · runs when this turn ends"),
        theme+": queued commands and ! runs must say they wait for the turn");
      check(contains(queued,"… +1 line")&&
        queued.text.find("why the parser owns")==std::string::npos,
        theme+": a long queued message must stop at three lines");
      check(contains(queued,"❯ Taken for delivery right now")&&
        queued.text.find("now   ")==std::string::npos,
        theme+": a delivering row is drawn as sent, without a label");
      check(contains(queued,"↑ edit queued")&&contains(queued,"to interrupt"),
        theme+": the status row must offer to edit queued poetry");
      const auto deliveredRow=deliveredScene(fixture);
      check(contains(deliveredRow,"Also run the lexer tests")&&
        deliveredRow.text.find("❯")!=std::string::npos&&
        (roles.basic||roles.monochrome||
         deliveredRow.styles.find("bg="+roles.userPanel.Print(true))!=std::string::npos),
        theme+": poetry delivered mid-turn must look like the user's message");
      const auto queuedInline=queuedInlineScene(fixture);
      check(contains(queuedInline,"❯ Also run the lexer tests before you finish  "
          "(queued · delivered at the next Gadget round)")&&
        contains(queuedInline,"❯ Taken for delivery right now")&&
        contains(queuedInline,"Running Gadgets…"),
        theme+": inline queued rows sit above the status line");
      const auto bats=batScenes(fixture);
      check(contains(bats.running,"2 Bats")&&contains(bats.running,"Research beta"),
        theme+": a finished or unlisted group keeps one row per Bat");
      check(contains(bats.launched,"2 Bats launched")&&
        bats.launched.text.find("Research")==std::string::npos,
        theme+": a group the list shows keeps a single line");
      if(!roles.monochrome){
        check(queued.styles.find("fg="+roles.muted.Print(false))!=std::string::npos&&
          queued.styles.find("fg="+roles.faint.Print(false))!=std::string::npos,
          theme+": queued rows must use the muted and faint roles");
        check(queued.styles.find("fg="+roles.userBar.Print(false))!=std::string::npos,
          theme+": the delivering row keeps the user prompt color");
      }else check(queued.styles.find(" d")!=std::string::npos,
        "monochrome queued rows must be dim");
      for(const auto& [scene,frame]:std::vector<std::pair<std::string,golden::Frame>>{
          {"queued",queued},{"queued-inline",queuedInline},
          {"bats-running",bats.running},{"bats-launched",bats.launched},
          {"welcome",welcome},{"transcript",transcript},{"gadgets",gadgets},
          {"permission",permission},{"input",input},{"providers-list",providers.list},
          {"providers-key",providers.key},{"providers-models",providers.models},
          {"providers-saved",providers.saved},{"missions-expanded",missions.expanded},
          {"missions-collapsed",missions.collapsed},{"missions-breadcrumb",missions.breadcrumb},
          {"shell-output",shellOutput}})
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
      check(contains(row,"Ctrl+B to run in background · Flight › test-audit"),
        "Bat status row must separate the notice from its breadcrumb");
    }
    {
      // The working row names what Esc does where the user is, and once Esc is
      // taken it shows the interrupt instead of offering it again.
      Fixture esc("batbox-dark");
      esc.state.loading=true;
      const auto row=[&](const conversation_shell::ViewState& view){
        return golden::render(esc.shell.chrome.renderStatus(view),120,1);};
      auto view=esc.view(120,24);view.streamMode="requesting";
      check(contains(row(view),"Connecting…")&&contains(row(view),"to interrupt"),
        "working row must offer Esc to interrupt from the input");
      view.transcript=true;
      check(contains(row(view),"to close transcript")&&
        row(view).text.find("to interrupt")==std::string::npos,
        "transcript view must say Esc closes it, not that it interrupts");
      view.transcript=false;view.selectedMission=std::string{};
      check(contains(row(view),"back to input")&&
        row(view).text.find("to interrupt")==std::string::npos,
        "Bats list focus must say Esc returns to the input");
      view.selectedMission.reset();view.interrupting=true;
      check(contains(row(view),"Interrupting…")&&
        row(view).text.find("Connecting…")==std::string::npos&&
        row(view).text.find("to interrupt")==std::string::npos,
        "an accepted Esc must show Interrupting… at once");
      esc.state.loading=false;
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
