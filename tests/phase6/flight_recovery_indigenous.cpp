#include "application_flights/Plans.hpp"
#include "flight_restore/FlightRestore.hpp"
#include "runtime/Platform.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::application_flights;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void write(const std::filesystem::path& file,const std::string& content){
  std::ofstream out(file,std::ios::binary);out<<content;
  if(!out)throw std::runtime_error("Test plan write failed");
}
Json todo(const std::string& text){
  return {{"content",text},{"status","in_progress"},{"activeForm","Working"}};
}
Json todoWrite(const Json& items){
  return {{"type","assistant"},{"message",{{"content",Json::array({
    Json{{"type","tool_use"},{"name","TodoWrite"},{"input",{{"todos",items}}}}
  })}}}};
}
}
int main(int argc,char** argv){
  if(argc==2&&std::string(argv[1])=="--probe"){
    const auto log=value::parse(std::string((std::istreambuf_iterator<char>(std::cin)),{}));
    const auto slug=planSlugFromLog(log),plan=recoverPlanFromMessages(log);
    const auto snapshot=findFileSnapshotEntry(value::property(log,"messages"),"plan");
    std::cout<<value::stringify(Json{{"slug",slug?Json(*slug):Json()},
      {"plan",plan?Json(*plan):Json()},{"snapshot",snapshot?*snapshot:Json()}})<<'\n';
    return 0;
  }
  const auto dir=std::filesystem::temp_directory_path()/
    ("batbox-session-recovery-"+platform::randomUUID());
  try{
    std::filesystem::create_directories(dir);
    int next=0;std::size_t errors=0;
    PlanFiles plans([&]{return dir;},[&](std::exception_ptr){++errors;},
      [&]{return "generated-"+std::to_string(++next);});
    const Json log={{"sessionId","resumed"},{"messages",Json::array({
      Json{{"type","user"},{"slug","original"},{"planContent","transcript plan"}}
    })}};
    write(dir/"original.md","latest saved plan");
    require(plans.resume(log,"resumed"),"Resume must reuse the saved slug");
    require(plans.content("resumed")=="latest saved plan","Disk plan must win over transcript text");
    require(plans.path("resumed","worker").filename()=="original-agent-worker.md",
      "Subagent plan must retain the original slug");
    require(plans.path("fresh")!=plans.path("resumed"),"Fresh sessions must not inherit project plans");
    require(!plans.content("fresh"),"Fresh sessions must have no saved plan content");
    require(plans.fork(log,"forked"),"Explicit fork must copy the saved plan");
    require(plans.path("forked")!=plans.path("resumed"),"Fork must isolate plan identity");
    write(plans.path("forked"),"fork progress");
    require(plans.content("resumed")=="latest saved plan","Fork edits must not alter original plan");
    plans.clear();
    require(plans.resume(log,"resumed"),"Restart must restore persisted slug");
    require(plans.path("resumed").filename()=="original.md","Restart must locate original file");
    write(dir/"legacy.md","legacy native plan");
    require(plans.resume(Json{{"messages",Json::array()}},"legacy"),
      "Legacy native session-id plan must remain recoverable");
    require(plans.content("legacy")=="legacy native plan","Legacy content changed");
    std::filesystem::remove(dir/"original.md");
    require(!plans.resume(log,"resumed"),"Local missing file must not silently revive transcript plan");
    require(!std::filesystem::exists(dir/"original.md"),"Local resume recreated missing plan");
    auto remote=log;
    remote["messages"].push_back({{"type","system"},{"subtype","file_snapshot"},
      {"snapshotFiles",Json::array({Json{{"key","plan"},{"content","snapshot plan"}}})}});
    require(plans.resume(remote,"resumed",true),"Remote recovery must restore snapshot");
    require(plans.content("resumed")=="snapshot plan","Latest file snapshot must win");
    std::filesystem::remove(dir/"original.md");
    remote["messages"].push_back({{"type","system"},{"subtype","file_snapshot"},
      {"snapshotFiles",Json::array()}});
    require(plans.resume(remote,"resumed",true),"Remote recovery must fall back to transcript");
    require(plans.content("resumed")=="transcript plan","Older snapshot must not override latest snapshot");
    auto history=log;
    history["messages"].push_back({{"type","attachment"},{"attachment",
      {{"type","plan_file_reference"},{"planContent","compacted plan"}}}});
    require(recoverPlanFromMessages(history)=="compacted plan","Compaction attachment recovery failed");
    history["messages"].push_back({{"type","assistant"},{"message",{{"content",
      Json::array({Json{{"type","tool_use"},{"name","ExitPlanMode"},
        {"input",{{"plan","approved plan"}}}}})}}}});
    require(recoverPlanFromMessages(history)=="approved plan","ExitPlanMode recovery failed");
    require(errors==0,"Unexpected filesystem recovery error");

    flight_restore::Services services;services.parseTodoList=flight_restore::parseTodoList;
    auto first=todo("first");first["extra"]="discarded";
    Json messages=Json::array({todoWrite(Json::array({first})),
      todoWrite(Json::array({todo("current")}))});
    require(flight_restore::extractTodosFromTranscript(messages,services)==
      Json::array({todo("current")}),"Resume must select the latest TodoWrite");
    require(flight_restore::parseTodoList(Json::array({first}))==Json::array({todo("first")}),
      "Todo parsing must strip fields outside the Todo schema");
    auto invalid=todo("invalid");invalid["activeForm"]="";
    messages.push_back(todoWrite(Json::array({invalid})));
    require(flight_restore::extractTodosFromTranscript(messages,services).empty(),
      "Invalid latest TodoWrite must not revive an older list");
    messages.push_back(todoWrite(Json::array()));
    require(flight_restore::extractTodosFromTranscript(messages,services).empty(),
      "Explicitly cleared todos must remain empty");
    std::filesystem::remove_all(dir);
    std::cout<<"session recovery: passed\n";return 0;
  }catch(const std::exception& error){
    std::filesystem::remove_all(dir);
    std::cerr<<error.what()<<'\n';return 1;
  }
}
