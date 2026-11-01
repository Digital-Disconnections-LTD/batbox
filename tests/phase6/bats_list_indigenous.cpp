#include "application_mission_ui/MissionDialog.hpp"
#include "interactive/presentation_providers/MissionFooter.hpp"
#include "runtime/Platform.hpp"
#include "shell/CommandSplitter.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace providers=batbox::interactive::presentation_providers;
namespace {
constexpr long long now=1'800'000'000'000LL;
void check(bool condition,const std::string& message){
  if(!condition)throw std::logic_error(message);
}
bool contains(const std::string& text,const std::string& part){
  return text.find(part)!=std::string::npos;
}
std::string joined(const interactive::UiRequest& request){
  std::string all=request.title+"\n"+request.body;
  for(const auto& choice:request.choices)all+="\n"+choice;
  return all;
}
void checkVocabulary(const interactive::UiRequest& request){
  const auto all=joined(request);
  for(const char* word:{"Agent","agent","Task","task"})
    check(!contains(all,word),std::string("Bats UI must use Batbox vocabulary, found '")+
      word+"' in:\n"+all);
}
Json missions(){
  return Json{
    {"bat-reviewer",{{"type","local_agent"},{"status","running"},
      {"name","reviewer"},{"description","Review the parser"},
      {"agentType","general-purpose"},{"model","fixture-small"},
      {"prompt","Review src/parser.cpp and report problems."},
      {"startTime",now-125000},{"isBackgrounded",true},
      {"progress",{{"toolUseCount",3},{"tokenCount",12345},
        {"lastText","Reading the parser now"},
        {"lastActivity",{{"toolName","Read"},{"activityDescription","Reading parser.cpp"}}},
        {"recentActivities",Json::array({Json{{"toolName","Grep"}},
          Json{{"toolName","Read"},{"activityDescription","Reading parser.cpp"}}})}}}}},
    {"bat-foreground",{{"type","local_agent"},{"status","running"},
      {"description","Foreground scan"},{"agentType","general-purpose"},
      {"startTime",now-5000},{"isBackgrounded",false},
      {"progress",{{"toolUseCount",1},{"tokenCount",900}}}}},
    {"bat-done",{{"type","local_agent"},{"status","completed"},
      {"description","Summarize history"},{"agentType","general-purpose"},
      {"startTime",now-600000},{"endTime",now-540000},{"evictAfter",now+1000},
      {"result",{{"totalTokens",4200},{"totalToolUseCount",7},{"totalDurationMs",60000}}}}},
    {"bat-leader",{{"type","local_agent"},{"status","running"},
      {"agentType","main-session"},{"description","Leader"},{"startTime",now}}}};
}
}
int main(){
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-bats-list-"+platform::randomUUID());
  std::filesystem::create_directories(root/"jobs");
  try{
    event_loop::Loop loop;
    auto state=std::make_shared<app_state::AppState>();
    state->fields=std::make_shared<value::SharedRecord>();
    state->fields->set("tasks",missions());
    auto store=std::make_shared<app_state::AppStateStore>(state);
    shell_missions::Services missionServices;
    missionServices.nowMs=[]{return now;};
    missionServices.enqueueNotification=[](const std::string&,const std::optional<std::string>&){};
    missionServices.onMissionTerminated=[](const shell_missions::MissionRecord&){};
    auto roster=std::make_shared<shell_missions::Roster>(missionServices);
    shell::FlightConfig shellConfig;shellConfig.shellPath="/bin/sh";
    shellConfig.originalCwd=root.string();shellConfig.outputDir=(root/"jobs").string();
    shellConfig.generateMissionId=[]{return std::string("shell-1");};
    shellConfig.splitCommand=[](const std::string& command){
      return shell_split::splitCommandWithOperators(command,[]{return std::string("salt");});};
    auto managed=std::make_shared<shell::ManagedShell>(std::move(shellConfig));

    std::optional<interactive::UiRequest> modal;
    std::string uiError;
    user_input::UiCommandRouter::Projection projection;
    projection.onUiChanged=[&](auto value){modal=std::move(value);};
    projection.onError=[&](const std::string& value){uiError=value;};
    projection.onQueueBlocked=[](bool){};
    projection.onPoetryFocus=[](bool){};
    auto router=user_input::UiCommandRouter::create(loop,std::move(projection));
    std::vector<std::string> opened;
    application_mission_ui::Inputs inputs(loop);
    inputs.store=store;inputs.shellMissions=roster;inputs.managedShell=managed;
    inputs.openBatConversation=[&](const std::string& id){opened.push_back(id);};
    inputs.stopBatMission=[](const std::string&){};
    inputs.dismissBatMission=[](const std::string&){};
    inputs.terminalRows=[]{return 40;};
    inputs.nowMillis=[]{return now;};

    auto bats=application_mission_ui::makeBatsCommand(router,inputs);
    check(bats.descriptor.at("name")=="bats"&&
      bats.descriptor.at("aliases")==Json::array({"agents"}),
      "The Bats command must be named bats with the agents alias");
    auto list=bats.invokeUi("");
    (void)list;
    for(int i=0;i<5;++i)loop.runReady();
    check(uiError.empty(),"Bats list reported an error: "+uiError);
    check(modal&&modal->kind==interactive::UiRequest::Kind::Select,"Bats list did not open");
    check(modal->title=="Bats","Bats list title must say Bats");
    checkVocabulary(*modal);
    const auto& rows=modal->choices;
    check(rows.size()==4,"Expected three Bats with one second line, got "+
      std::to_string(rows.size())+":\n"+joined(*modal));
    check(contains(rows[0],"Foreground scan")&&contains(rows[0],"running (foreground)")&&
      contains(rows[0],"5s")&&contains(rows[0],"1 Gadget ")&&contains(rows[0],"900 tokens"),
      "Newest running Bat must lead with its foreground state: "+rows[0]);
    check(contains(rows[1],"reviewer")&&contains(rows[1],"running")&&
      contains(rows[1],"2m 5s")&&contains(rows[1],"3 Gadgets")&&
      contains(rows[1],"12.3k tokens")&&contains(rows[1],"Reading parser.cpp"),
      "Named Bat row must show name, elapsed, Gadgets, tokens and Gadget: "+rows[1]);
    check(contains(rows[2],"“Reading the parser now”")&&rows[2].starts_with("    "),
      "Bat's latest reply must follow on an indented second line: "+rows[2]);
    check(contains(rows[3],"Summarize history")&&contains(rows[3],"completed")&&
      contains(rows[3],"1m")&&contains(rows[3],"7 Gadgets")&&contains(rows[3],"4.2k tokens"),
      "Settled Bat must show its final runtime and totals: "+rows[3]);
    check(!contains(joined(*modal),"Leader"),"The leader must not be listed as a Bat");
    check(modal->selectedIndex==0,"The first Bat must start selected");

    router->ui()->dispatchAction({interactive::UiAction::Kind::MoveSelection,1,"down"});
    check(modal&&modal->selectedIndex==1,"Down must select the second Bat");
    router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,"i"});
    check(modal&&modal->kind==interactive::UiRequest::Kind::Progress&&
      modal->title=="Bat · reviewer","i must open the selected Bat's details");
    for(const char* part:{"Name: reviewer","Model: fixture-small","Runtime: 2m 5s",
        "Gadget uses: 3","Recent Gadgets:","› Reading parser.cpp",
        "Latest reply: “Reading the parser now”","Review src/parser.cpp"})
      check(contains(modal->body,part),std::string("Bat details missing '")+part+"':\n"+modal->body);
    checkVocabulary(*modal);
    router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
    check(modal&&modal->kind==interactive::UiRequest::Kind::Select&&modal->selectedIndex==1,
      "Esc in Bat details must return to the list with the same selection");
    router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,"\x1b[C"});
    check(modal&&modal->kind==interactive::UiRequest::Kind::Progress,
      "Right arrow must also open Bat details");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Back,0,{}});
    check(modal&&modal->kind==interactive::UiRequest::Kind::Select,"Left must return to the list");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,1,{}});
    for(int i=0;i<5;++i)loop.runReady();
    check(opened==std::vector<std::string>{"bat-reviewer"},
      "Enter must open the selected Bat conversation");

    auto missionInputs=inputs;
    auto missionsCommand=application_mission_ui::makeMissionsCommand(router,missionInputs);
    check(missionsCommand.descriptor.at("name")=="missions"&&
      missionsCommand.descriptor.at("aliases")==Json::array({"tasks","bashes"}),
      "The Missions command must be named missions with tasks and bashes aliases");
    auto board=missionsCommand.invokeUi("");
    (void)board;
    for(int i=0;i<5;++i)loop.runReady();
    check(modal&&modal->kind==interactive::UiRequest::Kind::Select&&modal->title=="Missions",
      "Missions list did not open");
    checkVocabulary(*modal);
    const auto missionText=joined(*modal);
    check(contains(missionText,"Bat · Foreground scan")&&contains(missionText,"Bat · reviewer")&&
      contains(missionText,"2 active Bats")&&!contains(missionText,"Summarize history"),
      "Missions list must include running foreground and background Bats only:\n"+missionText);
    router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});

    const auto footerRows=providers::backgroundFooterMissions(missions());
    check(footerRows.size()==2&&footerRows[0].at("id")=="bat-foreground",
      "Footer must list both running Bats, newest first, and skip the leader");
    auto footer=providers::missionFooterAt(footerRows,100,24,ftxui::Color::Default,now);
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(100),ftxui::Dimension::Fixed(3));
    ftxui::Render(screen,footer);
    const auto rendered=screen.ToString();
    for(const char* part:{"Active Missions (2) · Ctrl+T Bats · /missions",
        "Foreground scan · foreground · 5s · 1 Gadget","reviewer · running · 2m 5s · 3 Gadgets",
        "“Reading the parser now”"})
      check(contains(rendered,part),std::string("Footer missing '")+part+"':\n"+rendered);
    check(!contains(rendered,"/Missions"),"Footer must not advertise the unresolvable /Missions");

    const auto glance=providers::batGlance(missions().at("bat-done"),now);
    check(glance.label=="Summarize history"&&glance.elapsed=="1m 0s"&&
      glance.gadgetUses==7&&glance.tokens==4200&&!glance.running,
      "Settled glance must read the result totals");
    check(providers::compactTokens(999)=="999 tokens"&&
      providers::compactTokens(123456)=="123k tokens"&&
      providers::compactDuration(3723000)=="1h 2m","Compact formatters changed");
    std::filesystem::remove_all(root);
    std::cout<<"Bats list rows, details, Missions list and footer render as expected\n";
    return 0;
  }catch(const std::exception& error){
    std::filesystem::remove_all(root);
    std::cerr<<error.what()<<'\n';return 1;
  }
}
