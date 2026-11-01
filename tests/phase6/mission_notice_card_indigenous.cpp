// A finished Bat's <task-notification> block renders in the leader transcript
// as one compact card, whether it started a turn or arrived mid-turn as a
// queued attachment. The message keeps the whole block for the model.
#include "GoldenFrame.hpp"
#include "application_presentation/Bindings.hpp"
#include "bats/BatMissionNotification.hpp"
#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "interactive/message_view/RetainedViews.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "normalization/Attachments.hpp"
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
bool has(const golden::Frame& frame,const std::string& text){
  return frame.text.find(text)!=std::string::npos;
}

// The block exactly as a finishing Bat enqueues it.
std::string batNotification(const Json& args){
  Json state={{"tasks",{{value::property(args,"taskId").get<std::string>(),
    Json{{"type","local_agent"},{"status","completed"}}}}}};
  std::string message;
  bats::BatMissionNotificationServices services;
  services.setAppState=[&state](const std::function<Json(const Json&)>& update){
    state=update(state);};
  services.abortSpeculation=[](const bats::RootSetAppState&){};
  services.getMissionOutputPath=[](const std::string& id){
    return "/home/bat/.batbox/tasks/"+id+".output";};
  services.enqueuePendingNotification=[&message](const Json& value){
    message=value.at("value").get<std::string>();};
  bats::enqueueBatNotification(args,services);
  check(!message.empty(),"The Bat notification was not enqueued");
  return message;
}

struct Fixture {
  event_loop::Loop loop;
  PresentationState state;
  terminal_ftxui::Options options;
  conversation_shell::Imports shell;
  ip::ThemeState theme;
  messages::Services messages;
  std::shared_ptr<ap::Bundle> bundle;
  explicit Fixture(const std::string& name):
      shell(loop,state,(options.interactive=false,options)),
      theme({[name]{return name;},[](const std::string&){},
        []{return std::string("dark");},[]{}},name){
    state.viewport.columns=80;state.viewport.rows=40;
    messages.strictGadgetResultPairing=[]{return false;};
    messages.featureGate=[](const std::string&){return false;};
    messages.logError=[](const std::string&){};
    ap::Inputs inputs;
    inputs.theme=&theme;inputs.messageServices=&messages;
    inputs.shortcuts.loadBindings=[]{return std::vector<ip::Binding>{};};
    inputs.gadgets=[]{return gadgets::Gadgets{};};inputs.commands=[]{return Json::array();};
    inputs.modelLabel=[]{return std::string("night-owl-7b");};
    inputs.workingDirectory=[]{return std::string("/home/bat/roost");};
    inputs.homeDirectory=[]{return std::string("/home/bat");};
    inputs.permissionMode=[]{return std::string("default");};
    inputs.statusText=[]{return std::string{};};
    inputs.gitBranch=[]{return std::string{};};
    inputs.contextUsage=[]()->std::optional<double>{return std::nullopt;};
    inputs.nowMs=[]{return 1234.0;};
    inputs.colors=name!="monochrome";
    bundle=ap::Bundle::bind(shell,std::move(inputs));
  }
  golden::Frame render(const std::vector<std::shared_ptr<Json>>& items){
    auto services=shell.projectionServices();
    services.lookups=std::make_shared<MessageLookups>();
    mv::MessageOptions options;options.columns=80;options.addMargin=true;
    auto imports=shell.messageImports();
    ftxui::Elements rows;
    for(const auto& item:items)
      if(auto element=mv::projectMessage(mv::prepareMessage(RenderItem(item),options,
          imports),services))rows.push_back(*element);
    return golden::render(ftxui::vbox(std::move(rows)),80,12);
  }
};

void parser(){
  const auto finished=batNotification({{"taskId","a1b2c3"},{"description","reviewer"},
    {"status","completed"},{"toolUseId","toolu_1"},
    {"finalMessage","## Review\nFound two parser bugs in the lexer.\nDetails follow."},
    {"usage",{{"totalTokens",4200},{"toolUses",3},{"durationMs",12400}}}});
  auto notices=gv::parseMissionNotifications(finished);
  check(notices.size()==1,"One block yields one notice");
  check(gv::missionNoticeText(notices[0])==
    "Bat reviewer finished · 3 Gadgets · 12s · Found two parser bugs in the lexer.",
    "Finished Bat text: "+gv::missionNoticeText(notices[0]));

  const auto failed=batNotification({{"taskId","f1"},{"description","fixer"},
    {"status","failed"},{"error","Model request failed: overloaded"}});
  notices=gv::parseMissionNotifications(failed);
  check(notices.size()==1&&gv::missionNoticeText(notices[0])==
    "Bat fixer failed · Model request failed: overloaded",
    "Failed Bat text: "+(notices.empty()?std::string{}:gv::missionNoticeText(notices[0])));

  const auto stopped=batNotification({{"taskId","s1"},{"description","scout \"north\""},
    {"status","killed"},{"finalMessage","Partial: read 3 files"}});
  notices=gv::parseMissionNotifications(stopped);
  check(notices.size()==1&&gv::missionNoticeText(notices[0])==
    "Bat scout \"north\" stopped · Partial: read 3 files",
    "Stopped Bat text: "+(notices.empty()?std::string{}:gv::missionNoticeText(notices[0])));

  const auto shell=std::string("<task-notification>\n<task-id>b9</task-id>\n"
    "<output-file>/tmp/b9.output</output-file>\n<status>failed</status>\n"
    "<summary>Background command \"make &amp; test\" failed with exit code 2</summary>\n"
    "</task-notification>");
  notices=gv::parseMissionNotifications(shell+"\n"+finished);
  check(notices.size()==2&&gv::missionNoticeText(notices[0])=="Shell make & test failed · exit 2",
    "Shell text: "+(notices.empty()?std::string{}:gv::missionNoticeText(notices[0])));
  check(gv::parseMissionNotifications("plain poetry").empty(),"Plain text has no notices");

  // A Bat whose queued input could not be continued uses the same card.
  const auto waiting=bats::batQueuedInputNotification("a1b2c3","reviewer",
    "Model request failed: overloaded");
  check(waiting.find("<task-id>a1b2c3</task-id>")!=std::string::npos&&
    waiting.find("Agent")==std::string::npos,"The queued-input block names the Bat: "+waiting);
  notices=gv::parseMissionNotifications(waiting);
  check(notices.size()==1&&gv::missionNoticeText(notices[0])==
    "Bat reviewer waiting · queued input awaiting resume · Model request failed: overloaded",
    "Queued-input text: "+(notices.empty()?std::string{}:gv::missionNoticeText(notices[0])));

  // Runs of finished shells collapse into one summary block.
  notices=gv::parseMissionNotifications("<task-notification><status>completed</status>"
    "<summary>3 background commands completed</summary></task-notification>");
  check(notices.size()==1&&gv::missionNoticeText(notices[0])=="3 background shells finished",
    "Collapsed shells text: "+(notices.empty()?std::string{}:gv::missionNoticeText(notices[0])));
}

void transcript(const std::string& theme){
  Fixture fixture(theme);
  const auto block=batNotification({{"taskId","a1b2c3"},{"description","reviewer"},
    {"status","completed"},{"toolUseId","toolu_1"},
    {"finalMessage","Found two parser bugs in the lexer.\n\nThe first is in Span."},
    {"usage",{{"totalTokens",4200},{"toolUses",3},{"durationMs",12400}}}});
  const auto expected="✓ Bat reviewer finished · 3 Gadgets · 12s · Found two parser bugs in the lexer.";

  // Idle leader: the notification starts a turn as a user message.
  auto started=std::make_shared<Json>(Json{{"type","user"},{"uuid","n1"},
    {"origin",{{"kind","task-notification"}}},
    {"message",{{"role","user"},{"content",Json::array({{{"type","text"},{"text",block}}})}}}});
  const auto copy=*started;
  const auto idle=fixture.render({started});
  check(has(idle,expected),theme+": the turn-starting notification must be one card:\n"+idle.text);
  check(!has(idle,"<task-notification>")&&!has(idle,"<summary>")&&!has(idle,"output-file"),
    theme+": raw notification XML leaked into the transcript:\n"+idle.text);
  check(*started==copy,"Rendering must not change the message the model reads");

  // Busy leader: the notification joins the running turn as a queued attachment.
  auto queued=std::make_shared<Json>(Json{{"type","attachment"},{"uuid","n2"},
    {"attachment",{{"type","queued_command"},{"commandMode","task-notification"},
      {"prompt",block}}}});
  const auto busy=fixture.render({queued});
  check(has(busy,expected),theme+": the queued notification must be one card:\n"+busy.text);
  check(!has(busy,"<task-notification>")&&!has(busy,"task-id"),
    theme+": raw queued notification XML leaked:\n"+busy.text);

  const auto& roles=ip::styleRoles(theme);
  if(!roles.monochrome)
    check(idle.styles.find("fg="+roles.gadgetOk.Print(false))!=std::string::npos,
      theme+": the finished mark must use the success role");

  // The queued-input notice renders as a card too.
  auto stuck=std::make_shared<Json>(Json{{"type","user"},{"uuid","n4"},
    {"origin",{{"kind","task-notification"}}},
    {"message",{{"role","user"},{"content",Json::array({{{"type","text"},
      {"text",bats::batQueuedInputNotification("a1","reviewer","overloaded")}}})}}}});
  const auto waiting=fixture.render({stuck});
  check(has(waiting,"■ Bat reviewer waiting · queued input awaiting resume · overloaded")&&
    !has(waiting,"<task-notification>"),theme+": queued-input notice must be a card:\n"+waiting.text);

  // A plain queued poetry line is still shown as written.
  auto plain=std::make_shared<Json>(Json{{"type","attachment"},{"uuid","n3"},
    {"attachment",{{"type","queued_command"},{"commandMode","prompt"},
      {"prompt","also check the docs"}}}});
  check(has(fixture.render({plain}),"also check the docs"),
    theme+": queued poetry must render unchanged");
}

void modelInput(){
  const auto block=batNotification({{"taskId","a1"},{"description","reviewer"},
    {"status","completed"},{"finalMessage","Found two parser bugs."},
    {"usage",{{"totalTokens",10},{"toolUses",1},{"durationMs",900}}}});
  const auto wrapped=normalization::wrapCommandText(block,Json{{"kind","task-notification"}});
  for(const char* part:{"<task-notification>","<task-id>a1</task-id>","<status>completed</status>",
      "<result>Found two parser bugs.</result>","<tool_uses>1</tool_uses>","</task-notification>"})
    check(wrapped.find(part)!=std::string::npos,
      std::string("The model must still receive '")+part+"':\n"+wrapped);
}
}

int main(){
  try{
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
    ip::applyColorDepth(ip::ColorDepth::TrueColor);
    parser();
    for(const std::string theme:{"batbox-dark","batbox-light","monochrome"})transcript(theme);
    modelInput();
    std::cout<<"Bat notifications render as compact cards and reach the model whole\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
