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
  for(const auto& info:request.choiceInfo)all+="\n"+info.detail+"\n"+info.badge;
  return all;
}
void checkVocabulary(const interactive::UiRequest& request){
  auto all=joined(request);
  // The Gadget that starts a Bat is really named Agent.
  for(auto at=all.find("Agent Gadget");at!=std::string::npos;at=all.find("Agent Gadget"))
    all.replace(at,12,"Bat Gadget");
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
    check(rows.size()==3&&modal->choiceInfo.size()==3,
      "Expected one selectable row per Bat, got "+
      std::to_string(rows.size())+":\n"+joined(*modal));
    check(contains(rows[0],"Foreground scan")&&contains(rows[0],"running (foreground)")&&
      contains(rows[0],"5s")&&contains(rows[0],"1 Gadget ")&&contains(rows[0],"900 tokens"),
      "Newest running Bat must lead with its foreground state: "+rows[0]);
    check(contains(rows[1],"reviewer")&&contains(rows[1],"running")&&
      contains(rows[1],"2m 5s")&&contains(rows[1],"3 Gadgets")&&
      contains(rows[1],"12.3k tokens")&&contains(rows[1],"Reading parser.cpp"),
      "Named Bat row must show name, elapsed, Gadgets, tokens and Gadget: "+rows[1]);
    check(modal->choiceInfo[1].detail=="“Reading the parser now”",
      "Bat's latest reply must be the row's dim detail line: "+modal->choiceInfo[1].detail);
    check(modal->choiceInfo[0].detail.empty(),
      "A Bat that has not spoken yet has no detail line: "+modal->choiceInfo[0].detail);
    check(contains(rows[2],"Summarize history")&&contains(rows[2],"completed")&&
      contains(rows[2],"1m")&&contains(rows[2],"7 Gadgets")&&contains(rows[2],"4.2k tokens"),
      "Settled Bat must show its final runtime and totals: "+rows[2]);
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

    {
      // With nothing running, /bats explains how to start one and lists the
      // configured types in their own section.
      for(int i=0;i<5;++i)loop.runReady();
      auto emptyState=std::make_shared<app_state::AppState>();
      emptyState->fields=std::make_shared<value::SharedRecord>();
      emptyState->fields->set("tasks",Json::object());
      auto emptyInputs=inputs;
      emptyInputs.store=std::make_shared<app_state::AppStateStore>(emptyState);
      emptyInputs.batDefinitions=[]{return Json{{"activeAgents",Json::array({
        Json{{"agentType","general-purpose"},{"whenToUse","Research and multi-step work"}},
        Json{{"agentType","reviewer"},{"whenToUse","Reviews a change for bugs"}}})}};};
      auto emptyBats=application_mission_ui::makeBatsCommand(router,emptyInputs);
      auto shown=emptyBats.invokeUi("");
      (void)shown;
      for(int i=0;i<5;++i)loop.runReady();
      check(modal&&modal->title=="Bats","Empty Bats list did not open");
      check(contains(modal->body,"0 Bats · 0 running")&&contains(modal->body,
        "No Bats yet · send one with the Agent Gadget or ask the leader to delegate"),
        "Empty Bats list must explain how to start a Bat:\n"+modal->body);
      check(modal->choices==std::vector<std::string>({"general-purpose","reviewer"})&&
        modal->choiceInfo.size()==2&&modal->choiceInfo[0].section=="Bat types"&&
        modal->choiceInfo[1].section=="Bat types"&&
        modal->choiceInfo[1].detail=="Reviews a change for bugs",
        "Configured types belong under Bat types:\n"+joined(*modal));
      check(!contains(joined(*modal),"Configuration ·"),
        "Bat types are listed by name:\n"+joined(*modal));
      checkVocabulary(*modal);
      router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
      for(int i=0;i<5;++i)loop.runReady();
    }

    const auto footerRows=providers::backgroundFooterMissions(missions());
    check(footerRows.size()==2&&footerRows[0].at("id")=="bat-foreground",
      "Footer must list both running Bats, newest first, and skip the leader");
    const auto render=[](const Json& rows,const providers::FooterOptions& options,int height){
      auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(options.columns),
        ftxui::Dimension::Fixed(height));
      ftxui::Render(screen,providers::missionFooterAt(rows,options));
      std::string plain;const auto styled=screen.ToString();
      for(std::size_t i=0;i<styled.size();++i){
        if(styled[i]!='\x1b'){plain+=styled[i];continue;}
        while(i<styled.size()&&!(styled[i]>='@'&&styled[i]<='~'&&styled[i]!='['))++i;
      }
      return plain;
    };
    providers::FooterOptions options;options.columns=100;options.terminalRows=24;options.nowMs=now;
    const auto rendered=render(footerRows,options,3);
    for(const char* part:{"Active Missions (2) · Ctrl+T collapse · /missions",
        "Foreground scan · foreground · 5s · 1 Gadget","reviewer · running · 2m 5s · 3 Gadgets",
        "“Reading the parser now”"})
      check(contains(rendered,part),std::string("Footer missing '")+part+"':\n"+rendered);
    check(!contains(rendered,"/Missions"),"Footer must not advertise the unresolvable /Missions");
    check(!contains(rendered,"▸"),"At home no Mission row is highlighted:\n"+rendered);

    // Sub-Bats sit under the Bat that launched them; a settled open Bat and
    // the Bats above it stay listed so the highlight has somewhere to be.
    auto tree=missions();
    tree["bat-child"]={{"type","local_agent"},{"status","running"},
      {"description","Check the lexer"},{"agentType","general-purpose"},
      {"parentBatId","bat-reviewer"},{"startTime",now-1000}};
    tree["bat-grandchild"]={{"type","local_agent"},{"status","completed"},
      {"description","Count tokens"},{"agentType","general-purpose"},
      {"parentBatId","bat-child"},{"startTime",now-900},{"endTime",now-100}};
    tree["shell-1"]={{"type","local_bash"},{"status","running"},
      {"description","npm test"},{"isBackgrounded",true},{"startTime",now}};
    tree["bat-loop-a"]={{"type","local_agent"},{"status","running"},
      {"description","Loop A"},{"parentBatId","bat-loop-b"},{"startTime",now-50}};
    tree["bat-loop-b"]={{"type","local_agent"},{"status","running"},
      {"description","Loop B"},{"parentBatId","bat-loop-a"},{"startTime",now-60}};
    const auto nested=providers::backgroundFooterMissions(tree,std::string("bat-grandchild"));
    std::vector<std::string> order;std::vector<long long> depths;
    for(const auto& row:nested){
      order.push_back(row.at("id").get<std::string>());
      depths.push_back(row.contains("depth")?row.at("depth").get<long long>():-1);
    }
    check(order==std::vector<std::string>{"bat-foreground","bat-reviewer","bat-child",
        "bat-grandchild","bat-loop-a","bat-loop-b","shell-1"}&&
      depths==std::vector<long long>{0,0,1,2,0,1,-1},
      "Bats must list as a tree before shells, with loops flattened: "+nested.dump());
    check(providers::footerBatIds(nested)==std::vector<std::string>{"bat-foreground",
        "bat-reviewer","bat-child","bat-grandchild","bat-loop-a","bat-loop-b"},
      "Only Bat rows are navigable");
    check(providers::batTrail(nested,"bat-grandchild")==std::vector<std::string>{
        "reviewer","Check the lexer","Count tokens"},"Trail runs from the top Bat down");
    check(providers::parentBatId(tree,"bat-grandchild")==std::optional<std::string>("bat-child")&&
      !providers::parentBatId(tree,"bat-reviewer"),"Parent lookup");
    check(providers::backgroundFooterMissions(tree).size()==nested.size()-1,
      "A settled Bat leaves the list once it is no longer open");

    options.current="bat-grandchild";options.terminalRows=40;
    const auto highlighted=render(nested,options,7);
    check(contains(highlighted,"  ● reviewer · running")&&
      contains(highlighted,"  └ ● Check the lexer · running")&&
      contains(highlighted,"▸   └ ● Count tokens · completed")&&
      contains(highlighted,"Active Missions (6)"),
      "Nested rows indent under their parent and the open Bat is marked:\n"+highlighted);
    const auto window=providers::footerWindow(nested,40,false,std::string("bat-loop-b"));
    check(!window.collapsed&&window.begin==1&&window.end==6,
      "The window scrolls to keep the open Bat in view");
    check(providers::footerWindow(nested,40,false,std::nullopt).begin==0&&
      providers::footerWindow(nested,40,false,std::nullopt).end==5,"At most five rows show");
    check(providers::footerWindow(nested,11,false,std::nullopt).collapsed,
      "A short terminal collapses the list");
    options.collapsed=true;
    const auto collapsed=render(nested,options,1);
    check(contains(collapsed,"Active Missions (6) · in: Count tokens")&&
      !contains(collapsed,"reviewer"),"Collapsed list names the open Bat:\n"+collapsed);

    // The collapsed line counts every running Bat in the tree, sub-Bats
    // included, and a stopped Bat reads as stopped.
    Json chain=Json::object();
    const auto bat=[](const std::string& description,const std::string& parent,int age){
      Json row={{"type","local_agent"},{"status","running"},{"agentType","general-purpose"},
        {"description",description},{"startTime",now-age}};
      if(!parent.empty())row["parentBatId"]=parent;
      return row;
    };
    chain["bat-audit"]=bat("Audit the parser","",3000);
    chain["bat-lexer"]=bat("Check the lexer","bat-audit",2000);
    chain["bat-tokens"]=bat("Count the tokens","bat-lexer",1000);
    options.current="bat-tokens";
    const auto chainRows=providers::backgroundFooterMissions(chain,std::string("bat-tokens"));
    check(contains(render(chainRows,options,1),"Active Missions (3) · in: Count the tokens"),
      "Collapsed count must include sub-Bats:\n"+render(chainRows,options,1));
    for(const auto* id:{"bat-audit","bat-lexer","bat-tokens"})chain[id]["status"]="killed";
    options.collapsed=false;
    const auto stopped=render(providers::backgroundFooterMissions(chain,std::string("bat-tokens")),
      options,4);
    check(contains(stopped,"Active Missions (0)")&&contains(stopped,"  ● Audit the parser · stopped")&&
      contains(stopped,"  └ ● Check the lexer · stopped")&&
      contains(stopped,"▸   └ ● Count the tokens · stopped"),
      "Stopped Bats in the open trail read as stopped:\n"+stopped);

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
