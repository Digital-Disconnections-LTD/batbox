#include "application_commands/FlightTitles.hpp"
#include "application_flights/Restore.hpp"
#include "commands_indigenous/UiCommands.hpp"
#include "flights/FlightListing.hpp"
#include "flights/Load.hpp"
#include "flights/Transcript.hpp"
#include "messages/RuntimeMessages.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace batbox;
namespace {
const std::filesystem::path fixtures=BATBOX_FLIGHT_FIXTURES;
void check(bool value,const std::string& message){
  if(!value)throw std::runtime_error(message);
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));return result;
}
template<class T>event_loop::Deferred<T> failed(event_loop::Loop& loop,const char* text){
  event_loop::Deferred<T> result(loop);
  result.reject(std::make_exception_ptr(std::runtime_error(text)));return result;
}
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr failure;
  pending.then([&](const T* result,const std::exception_ptr& error){
    if(result)value=*result;
    failure=error;
  });
  for(int i=0;i<60&&!value&&!failure;++i)loop.runReady();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Deferred did not settle");
  return *value;
}
flights::PoetryServices poetry(){
  flights::PoetryServices services;
  services.extractTag=messages::extractTag;
  services.builtInCommandNames=[]{return std::set<std::string>{};};
  return services;
}
std::optional<Json> scan(const std::filesystem::path& file,const std::string& id){
  auto services=poetry();
  return flights::scanFlightFileLite(runtime::pathToUtf8(file),id,1000.0,services);
}
std::string text(const Json& log,const char* key){
  const auto& value=value::property(log,key);
  return value.is_string()?value.get<std::string>():std::string("<missing>");
}
double at(const std::string& stamp){
  const auto value=flights::isoTimestampMs(stamp);
  if(!value)throw std::runtime_error("bad test timestamp "+stamp);
  return *value;
}
Json entry(const std::string& type,const std::string& uuid,const std::string& parent,
    const Json& content,const std::string& stamp){
  Json message={{"role",type},{"content",content}};
  if(type=="assistant")message["model"]="claude-sonnet-4-5-20250929";
  return Json{{"parentUuid",parent.empty()?Json():Json(parent)},{"isSidechain",false},
    {"cwd","/work/big"},{"sessionId","66666666-6666-4666-8666-666666666666"},
    {"slug","steady-humming-river"},{"type",type},{"message",message},
    {"uuid",uuid},{"timestamp",stamp}};
}

void liteScanFixtures(){
  const auto normal=scan(fixtures/"normal.jsonl","11111111-1111-4111-8111-111111111111");
  check(normal.has_value(),"normal flight must list");
  check(text(*normal,"firstPrompt")=="Fix the login button on mobile It does nothing when tapped.",
    "first prompt excerpt: "+text(*normal,"firstPrompt"));
  check(text(*normal,"slug")=="dazzling-whistling-tide","slug from transcript");
  check(text(*normal,"gitBranch")=="fix/login","latest git branch wins");
  check(text(*normal,"model")=="claude-sonnet-4-5-20250929","last assistant model");
  check(text(*normal,"projectPath")=="/work/app","project path from first entry");
  check(text(*normal,"endState")=="ended","flight-end marker after the last reply");
  check(value::property(*normal,"messageCount")==4,"exact count for a small file");
  check(value::property(*normal,"messageCountApproximate")==false,"small files count exactly");
  check(value::property(*normal,"messages").empty(),"listing rows stay lite");
  check(value::property(*normal,"lastActivityMs")==at("2026-10-30T09:01:30.000Z"),
    "last activity is the newest message timestamp");
  check(text(*normal,"lastAssistantText")=="Patched: the handler now listens for pointer events.",
    "last assistant text");

  const auto titled=scan(fixtures/"custom-title.jsonl","22222222-2222-4222-8222-222222222222");
  check(titled.has_value(),"custom-title flight must list");
  check(text(*titled,"customTitle")=="Rate limiter rewrite","custom title read");
  check(text(*titled,"aiTitle")=="Review API rate limiter","ai title read");
  check(flights::flightListingTitle(*titled)=="Rate limiter rewrite","custom title beats ai title");
  check(text(*titled,"endState")=="interrupted","unresolved tool use marks interruption");
  check(value::property(*titled,"messageCount")==2,"tool-use-only reply is not visible");

  const auto truncated=scan(fixtures/"truncated.jsonl","33333333-3333-4333-8333-333333333333");
  check(truncated.has_value(),"a torn final line must not hide the flight");
  check(value::property(*truncated,"messageCount")==2,"torn line is skipped");
  check(text(*truncated,"endState")=="","complete reply before the torn line");
  check(text(*truncated,"model")=="claude-opus-4-1","model survives the torn line");

  const auto missing=scan(fixtures/"missing-timestamp.jsonl","44444444-4444-4444-8444-444444444444");
  check(missing.has_value(),"missing timestamp must not hide the flight");
  check(text(*missing,"firstPrompt")=="Explain the cache layer","prompt without timestamp");
  check(text(*missing,"endState")=="interrupted","interruption marker detected");
  check(value::property(*missing,"lastActivityMs")==at("2026-10-28T12:00:30.000Z"),
    "activity from the newest stamped entry");

  check(!scan(fixtures/"metadata-only.jsonl","55555555-5555-4555-8555-555555555555"),
    "metadata-only files are not resumable");

  // The write queue can persist a prompt after its reply; the end state comes
  // from the parentUuid chain, not from file order.
  const auto reordered=scan(fixtures/"reordered.jsonl","88888888-8888-4888-8888-888888888888");
  check(reordered.has_value(),"reordered flight must list");
  check(text(*reordered,"endState")=="ended","reply before prompt on disk still ended: "+
    text(*reordered,"endState"));
  check(text(*reordered,"firstPrompt")=="Port the expression parser to C++20",
    "prompt found after its reply");
  check(value::property(*reordered,"lastActivityMs")==at("2026-10-30T10:00:05.100Z"),
    "newest timestamp, not last line");
}

void liteScanGenerated(){
  const auto dir=std::filesystem::temp_directory_path()/
    ("batbox-flight-listing-"+platform::randomUUID());
  std::filesystem::create_directories(dir);
  try{
    const auto commands=dir/"commands.jsonl";
    {
      std::ofstream out(commands,std::ios::binary);
      out<<value::stringify(Json{{"type","user"},{"isMeta",true},{"uuid","m1"},
        {"timestamp","2026-10-01T00:00:00.000Z"},{"message",{{"role","user"},
        {"content","<local-command-caveat>Caveat</local-command-caveat>"}}}})<<'\n';
      out<<value::stringify(Json{{"type","user"},{"uuid","m2"},{"parentUuid","m1"},
        {"timestamp","2026-10-01T00:00:01.000Z"},{"message",{{"role","user"},
        {"content","<command-name>/exit</command-name>\n<command-message>exit</command-message>"}}}})<<'\n';
    }
    check(!scan(commands,"77777777-7777-4777-8777-777777777777"),
      "a Flight with only command echoes is not listed");

    const auto big=dir/"big.jsonl";
    std::size_t written=0,visible=0;
    {
      std::ofstream out(big,std::ios::binary);
      out<<value::stringify(entry("user","u0","",Json("Port the parser to C++20"),
        "2026-10-01T00:00:00.000Z"))<<'\n';
      ++visible;
      const std::string filler(900,'x');
      std::string parent="u0";
      for(int i=1;i<=400;++i){
        const auto uuid="a"+std::to_string(i);
        out<<value::stringify(entry("assistant",uuid,parent,Json::array({
          Json{{"type","text"},{"text","step "+std::to_string(i)+" "+filler}}}),
          "2026-10-01T00:10:00.000Z"))<<'\n';
        parent=uuid;++visible;
      }
      out<<value::stringify(Json{{"type","custom-title"},{"customTitle","Parser port"},
        {"sessionId","66666666-6666-4666-8666-666666666666"}})<<'\n';
      out<<value::stringify(entry("assistant","last",parent,Json::array({
        Json{{"type","tool_use"},{"id","t1"},{"name","Bash"},{"input",Json::object()}}}),
        "2026-10-01T01:00:00.000Z"))<<'\n';
      written=static_cast<std::size_t>(out.tellp());
    }
    check(written>3*65536,"generated transcript is larger than the scan window");
    const auto log=scan(big,"66666666-6666-4666-8666-666666666666");
    check(log.has_value(),"large flight must list");
    check(text(*log,"firstPrompt")=="Port the parser to C++20","prompt from the head");
    check(text(*log,"customTitle")=="Parser port","title from the tail");
    check(text(*log,"endState")=="interrupted","end state from the tail");
    check(value::property(*log,"messageCountApproximate")==true,"large files estimate");
    const auto estimate=value::property(*log,"messageCount").get<double>();
    check(estimate>visible*0.5&&estimate<visible*1.5,
      "estimate within 50% (got "+std::to_string(estimate)+")");
    const auto label=flights::flightListingLabel(*log,at("2026-10-01T03:00:00.000Z"));
    check(label.find("~")!=std::string::npos,"estimated counts are marked: "+label);
    std::filesystem::remove_all(dir);
  }catch(...){std::filesystem::remove_all(dir);throw;}
}

void labels(){
  const auto now=at("2026-11-01T12:00:00.000Z");
  const struct {double seconds;const char* text;} times[]={
    {0,"just now"},{30,"just now"},{60,"1 minute ago"},{150,"3 minutes ago"},
    {3600,"1 hour ago"},{7200,"2 hours ago"},{86400,"yesterday"},
    {3*86400,"3 days ago"},{8*86400,"1 week ago"},{20*86400,"3 weeks ago"},
    {60*86400,"2 months ago"},{800*86400,"2 years ago"},{-60,"just now"}};
  for(const auto& item:times){
    const auto value=flights::relativeTimeText(now-item.seconds*1000,now);
    check(value==item.text,"relative time for "+std::to_string(item.seconds)+"s: "+value);
  }
  check(flights::shortModelName("claude-sonnet-4-5-20250929")=="sonnet-4-5","dated model");
  check(flights::shortModelName("local/qwen3-coder")=="qwen3-coder","provider prefix");
  check(flights::shortModelName("claude-opus-4-1@20250805")=="opus-4-1","vertex date");
  check(flights::shortModelName("gpt")=="gpt","short names stay");

  const auto normal=scan(fixtures/"normal.jsonl","11111111-1111-4111-8111-111111111111");
  const auto label=flights::flightListingLabel(*normal,at("2026-10-30T11:01:30.000Z"));
  check(label=="Fix the login button on mobile It does nothing when tapped. · 2 hours ago"
    " · fix/login · sonnet-4-5 · 4 messages · ended  (dazzling-whistling-tide)",
    "normal label: "+label);
  const auto titled=scan(fixtures/"custom-title.jsonl","22222222-2222-4222-8222-222222222222");
  const auto second=flights::flightListingLabel(*titled,at("2026-10-31T10:00:34.000Z"));
  check(second=="Rate limiter rewrite · just now · main · qwen3-coder · 2 messages"
    " · interrupted  (quiet-orbiting-heron)","titled label: "+second);
  Json legacy={{"sessionId","3f2a9c1b-0000-4000-8000-000000000000"},
    {"firstPrompt",std::string(90,'a')}};
  const auto clipped=flights::flightListingTitle(legacy);
  check(value::stringLength(clipped)==60&&clipped.ends_with("…"),"long titles clip to 60");
  Json bare={{"sessionId","3f2a9c1b-0000-4000-8000-000000000000"}};
  check(flights::flightListingLabel(bare,now)=="3f2a9c1b","bare logs fall back to a short id");
}

void matching(){
  Json logs=Json::array();
  logs.push_back(*scan(fixtures/"normal.jsonl","11111111-1111-4111-8111-111111111111"));
  logs.push_back(*scan(fixtures/"custom-title.jsonl","22222222-2222-4222-8222-222222222222"));
  logs.push_back(*scan(fixtures/"truncated.jsonl","33333333-3333-4333-8333-333333333333"));
  logs.push_back(*scan(fixtures/"missing-timestamp.jsonl","44444444-4444-4444-8444-444444444444"));
  using Indices=std::vector<std::size_t>;
  check(flights::matchFlightLogs(logs,"dazzling-whistling-tide")==Indices{0},"slug match");
  check(flights::matchFlightLogs(logs,"Rate Limiter Rewrite")==Indices{1},"title match");
  check(flights::matchFlightLogs(logs,"11111111")==Indices{0},"short id prefix");
  check(flights::matchFlightLogs(logs,"login")==Indices{0},"word match");
  check(flights::matchFlightLogs(logs,"/work/app")==(Indices{0,2,3}),"project match");
  check(flights::matchFlightLogs(logs,"cache lru")==Indices{},"all words must match");
  check(flights::matchFlightLogs(logs,"  ").empty(),"blank queries match nothing");
}

void pickerFilter(){
  using commands_indigenous::applyFilterKeys;
  using commands_indigenous::filterMatches;
  check(applyFilterKeys("","l")=="l","typing appends");
  check(applyFilterKeys("lo","\x7f")=="l","backspace removes one character");
  check(applyFilterKeys("caf\xc3\xa9","\x7f")=="caf","backspace removes a whole UTF-8 character");
  check(applyFilterKeys("abc","\x1b[A")=="abc","arrow sequences do not edit");
  check(applyFilterKeys("abc","\x15")=="","ctrl-u clears");
  check(applyFilterKeys("","\x7f")=="","backspace on empty filter");
  check(filterMatches("fix the login · main","LOGIN main"),"case-insensitive words");
  check(!filterMatches("fix the login · main","login prod"),"every word must match");
  check(filterMatches("anything",""),"empty filter matches");
}

std::shared_ptr<commands_indigenous::ResumeServices> pickerServices(
    event_loop::Loop& loop,Json logs,std::string& resumedAt){
  auto services=std::make_shared<commands_indigenous::ResumeServices>(loop);
  services->originalCwd=[]{return std::string("/project");};
  services->currentFlightId=[]{return std::string("live");};
  services->worktreePaths=[&loop](const std::string&){
    return ready(loop,std::vector<std::string>{"/project"});};
  services->loadSameRepoLogs=[&loop,logs](const std::vector<std::string>&){
    return ready(loop,logs);};
  services->loadAllProjectsLogs=[&loop]{return ready(loop,Json::array());};
  services->flightIdFromLog=[](const Json& log){return log.at("sessionId").get<std::string>();};
  services->logLabel=[](const Json& log){return flights::flightListingLabel(log,0);};
  services->searchText=flights::flightSearchText;
  services->matchLogs=flights::matchFlightLogs;
  services->isLiteLog=[](const Json&){return false;};
  services->loadFullLog=[&loop](const Json& log){return ready(loop,log);};
  services->validateUuid=[](const std::string& value)->std::optional<std::string>{
    return value.empty()?std::nullopt:std::optional(value);};
  services->modifiedTimeMs=[](const Json&){return 0.0;};
  services->getLastFlightLog=[&loop](const std::string&){
    return ready(loop,std::optional<Json>{});};
  services->customTitleEnabled=[]{return true;};
  services->searchByCustomTitle=[&loop](const std::string&,bool){
    return ready(loop,Json::array());};
  services->checkCrossProject=[](const Json&,bool,const std::vector<std::string>&){
    return commands_indigenous::CrossProject{};};
  services->resume=[&loop,&resumedAt](const std::string& id,const Json&,
      const std::string& entrypoint){
    resumedAt=id+":"+entrypoint;return ready(loop,event_loop::Unit{});};
  return services;
}

void pickerFlow(){
  event_loop::Loop loop;
  std::optional<interactive::UiRequest> view;
  auto router=user_input::UiCommandRouter::create(loop,{
    [&](std::optional<interactive::UiRequest> next){view=std::move(next);},
    [](const std::string& failure){throw std::runtime_error(failure);},
    [](bool){},[](bool){}
  });
  Json logs=Json::array();
  for(int i=0;i<60;++i){
    const auto id="flight-"+std::to_string(i);
    logs.push_back(Json{{"sessionId",id},{"messages",Json::array()},
      {"firstPrompt",(i==42?"Tune the beta cache":"Routine task ")+std::to_string(i)},
      {"slug","slug-"+std::to_string(i)}});
  }
  std::string resumedAt;
  auto services=pickerServices(loop,logs,resumedAt);
  auto resume=commands_indigenous::makeResumeCommand(router,services);
  check(!resume.descriptor.contains("aliases"),"/continue is its own command");
  auto picked=resume.invokeUi("");
  for(int i=0;i<20&&(!view||view->id!="resume");++i)loop.runReady();
  check(view&&view->id=="resume","picker visible");
  check(view->choices.size()==52,"50 rows, a more row and the scope toggle (got "+
    std::to_string(view->choices.size())+")");
  check(view->choices[50]=="… 10 more, keep typing to narrow","more row text: "+view->choices[50]);
  check(view->bodyPreviewLines==std::optional<std::size_t>(2),"bounded picker body");
  check(view->body.find("Type to filter")!=std::string::npos,"key help in body");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,50,{}});
  loop.runReady();
  check(resumedAt.empty()&&view&&view->id=="resume","the more row is inert");
  for(const char c:std::string("bet"))
    router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,std::string(1,c)});
  loop.runReady();
  check(view->choices.size()==2,"filter narrows to one row plus toggle");
  check(view->body.find("filter: bet")!=std::string::npos,"body shows the filter");
  router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,"qq"});
  loop.runReady();
  check(view->choices.size()==2&&view->choices[0]=="No Flights match \"betqq\"",
    "no-match row");
  router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,"\x7f"});
  router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,"\x7f"});
  router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  const auto completion=await(loop,picked);
  check(completion.display=="skip"&&resumedAt=="flight-42:slash_command_picker",
    "filtered selection resumes the matching Flight: "+resumedAt);

  resumedAt.clear();
  const auto bySlug=await(loop,resume.invokeUi("slug-7"));
  check(bySlug.display=="skip"&&resumedAt=="flight-7:slash_command_title",
    "/resume <slug> resumes: "+resumedAt);
  resumedAt.clear();
  const auto ambiguous=await(loop,resume.invokeUi("routine task"));
  check(resumedAt.empty()&&ambiguous.result&&
    ambiguous.result->find("Found 59 Flights matching routine task:")==0,
    "ambiguous names list matches");

  services->latestLog=[&loop,logs]{return ready(loop,std::optional<Json>(logs[3]));};
  auto continued=commands_indigenous::makeContinueCommand(router,services);
  check(continued.descriptor.at("name")=="continue","continue registration");
  const auto latest=await(loop,continued.invokeUi(""));
  check(latest.display=="skip"&&resumedAt=="flight-3:slash_command_continue",
    "/continue resumes the newest Flight");
  services->latestLog=[&loop]{return ready(loop,std::optional<Json>{});};
  const auto none=await(loop,continued.invokeUi(""));
  check(none.result=="No saved Flight to continue in this project","/continue with nothing saved");
}

void pickerRowParts(){
  const auto normal=scan(fixtures/"normal.jsonl","11111111-1111-4111-8111-111111111111");
  const auto now=at("2026-10-30T11:01:30.000Z");
  check(flights::flightListingDetail(*normal,now)==
    "dazzling-whistling-tide · fix/login · sonnet-4-5 · 2 hours ago · 4 messages",
    "detail line: "+flights::flightListingDetail(*normal,now));
  check(flights::flightListingEndState(*normal)=="ended","clean exit badge");
  const auto titled=scan(fixtures/"custom-title.jsonl","22222222-2222-4222-8222-222222222222");
  check(flights::flightListingEndState(*titled)=="interrupted","interrupted badge");
  Json bare={{"sessionId","3f2a9c1b-0000-4000-8000-000000000000"}};
  check(flights::flightListingDetail(bare,now).empty()&&
    flights::flightListingEndState(bare).empty(),"unknown parts are left out");

  // The picker shows the title as the row and the rest as its detail line.
  event_loop::Loop loop;
  std::optional<interactive::UiRequest> view;
  auto router=user_input::UiCommandRouter::create(loop,{
    [&](std::optional<interactive::UiRequest> next){view=std::move(next);},
    [](const std::string& failure){throw std::runtime_error(failure);},
    [](bool){},[](bool){}
  });
  Json logs=Json::array({*normal,*titled});
  std::string resumedAt;
  auto services=pickerServices(loop,logs,resumedAt);
  services->logLabel=flights::flightListingTitle;
  services->logChoice=[now](const Json& log){
    interactive::UiChoice choice;
    choice.detail=flights::flightListingDetail(log,now);
    choice.badge=flights::flightListingEndState(log);
    return choice;
  };
  auto resume=commands_indigenous::makeResumeCommand(router,services);
  auto picked=resume.invokeUi("");
  for(int i=0;i<20&&(!view||view->id!="resume");++i)loop.runReady();
  check(view&&view->id=="resume","picker visible");
  check(view->choices.size()==3&&view->choiceInfo.size()==3,
    "two Flights and the scope toggle, each with presentation");
  check(view->choices[0]==flights::flightListingTitle(*normal),
    "the title is the main row: "+view->choices[0]);
  check(view->choiceInfo[0].detail==flights::flightListingDetail(*normal,now)&&
    view->choiceInfo[0].badge=="ended","slug, branch, model and time are the detail line");
  check(view->choiceInfo[1].badge=="interrupted","end state is the badge");
  check(view->choiceInfo[2].detail.empty()&&view->choiceInfo[2].badge.empty(),
    "the toggle row stays plain");
  for(const char c:std::string("qwen3"))
    router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,std::string(1,c)});
  loop.runReady();
  check(view->choices.size()==2&&view->choices[0]==flights::flightListingTitle(*titled),
    "the filter matches detail text such as the model");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  const auto completion=await(loop,picked);
  check(completion.display=="skip"&&
    resumedAt=="22222222-2222-4222-8222-222222222222:slash_command_picker",
    "picking the filtered row resumes it: "+resumedAt);
}

void pickerAllProjectsSwitch(){
  event_loop::Loop loop;
  std::optional<interactive::UiRequest> view;
  auto router=user_input::UiCommandRouter::create(loop,{
    [&](std::optional<interactive::UiRequest> next){view=std::move(next);},
    [](const std::string& failure){throw std::runtime_error(failure);},
    [](bool){},[](bool){}
  });
  Json local=Json::array({Json{{"sessionId","here-1"},{"messages",Json::array()},
    {"firstPrompt","Local work"},{"projectPath","/project"}}});
  Json all=Json::array({Json{{"sessionId","there-1"},{"messages",Json::array()},
    {"firstPrompt","Elsewhere work"},{"projectPath","/elsewhere"}}});
  std::string resumedAt;
  auto services=pickerServices(loop,local,resumedAt);
  services->loadAllProjectsLogs=[&loop,all]{return ready(loop,all);};
  services->projectLabel=[](const Json& log){return log.at("projectPath").get<std::string>();};
  services->logLabel=flights::flightListingTitle;
  services->logChoice=[](const Json& log){
    interactive::UiChoice choice;choice.detail=flights::flightListingDetail(log,0);
    return choice;};
  services->checkCrossProject=[](const Json& log,bool allProjects,const std::vector<std::string>&){
    commands_indigenous::CrossProject cross;
    if(allProjects&&log.at("projectPath")!="/project"){
      cross.isCrossProject=true;cross.canSwitchInProcess=true;cross.projectPath="~/elsewhere";
    }
    return cross;};
  auto resume=commands_indigenous::makeResumeCommand(router,services);
  auto picked=resume.invokeUi("");
  for(int i=0;i<20&&(!view||view->id!="resume");++i)loop.runReady();
  check(view&&view->choices.size()==2,"local picker shows one Flight and the toggle");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,1,{}});
  const auto showsElsewhere=[&]{
    return view&&!view->choices.empty()&&view->choices.front()=="Elsewhere work";};
  for(int i=0;i<20&&!showsElsewhere();++i)loop.runReady();
  check(showsElsewhere()&&!view->choiceInfo.empty()&&
    view->choiceInfo.front().detail.find("/elsewhere")!=std::string::npos,
    "All projects rows show their project path in the detail line");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  const auto completion=await(loop,picked);
  check(resumedAt=="there-1:slash_command_picker"&&
    completion.result==std::optional<std::string>("Switched Roost to ~/elsewhere"),
    "switching projects from the picker says where the Roost went: "+
      completion.result.value_or("<none>"));
}

void continueSkipsBadFiles(){
  event_loop::Loop loop;
  const Json records=Json::array({Json{{"id","broken"}},Json{{"id","empty"}},
    Json{{"id","good"}},Json{{"id","older"}}});
  std::vector<std::string> tried;
  const auto loader=[&](const Json& item)->event_loop::Deferred<std::optional<Json>>{
    const auto id=item.at("id").get<std::string>();
    tried.push_back(id);
    if(id=="broken")return failed<std::optional<Json>>(loop,"Unexpected end of JSON");
    if(id=="empty")return ready(loop,std::optional<Json>{});
    return ready(loop,std::optional<Json>(Json{{"sessionId",id}}));
  };
  const auto chosen=await(loop,application_flights::firstLoadable(loop,records,loader));
  check(chosen&&chosen->at("sessionId")=="good","a failing file is skipped, not fatal");
  check(tried==std::vector<std::string>({"broken","empty","good"}),"stops at the first loadable");
  bool rejected=false;
  try{
    (void)await(loop,application_flights::firstLoadable(loop,
      Json::array({Json{{"id","broken"}}}),loader));
  }catch(const std::exception& error){
    rejected=std::string(error.what()).find("1 file unreadable: Unexpected end of JSON")!=
      std::string::npos;
  }
  check(rejected,"all-unreadable reports the reason");
  const auto nothing=await(loop,application_flights::firstLoadable(loop,Json::array(),loader));
  check(!nothing,"no records resolves empty");
  bool throwing=false;
  const auto recovered=await(loop,application_flights::firstLoadable(loop,
    Json::array({Json{{"id","throws"}},Json{{"id","good"}}}),
    [&](const Json& item)->event_loop::Deferred<std::optional<Json>>{
      if(item.at("id")=="throws"){throwing=true;throw std::runtime_error("bad path");}
      return ready(loop,std::optional<Json>(Json{{"sessionId","good"}}));
    }));
  check(throwing&&recovered&&recovered->at("sessionId")=="good","a throwing loader is skipped");
}

void missingTimestampChain(){
  std::ifstream input(fixtures/"missing-timestamp.jsonl",std::ios::binary);
  std::stringstream buffer;buffer<<input.rdbuf();
  const auto entries=flights::parseJSONLBuffer(buffer.str());
  flights::GraphServices graph;
  graph.parseTimestamp=[](const std::string& value){
    return flights::isoTimestampMs(value).value_or(0);};
  graph.localeCompare=[](const std::string& a,const std::string& b){return a.compare(b);};
  graph.logError=[](const std::string&){};
  const auto leaf=flights::findLatestMessage(entries,[](const Json&){return true;},graph);
  check(leaf&&leaf->at("uuid")=="d0000000-0000-4000-8000-000000000003",
    "latest message found despite a missing timestamp");
  const auto lone=flights::findLatestMessage(Json::array({entries[0]}),
    [](const Json&){return true;},graph);
  check(lone&&lone->at("uuid")=="d0000000-0000-4000-8000-000000000001",
    "an unstamped message can still be the leaf");
  const flights::MessageMap map(entries);
  const auto chain=flights::buildConversationChain(map,*leaf,graph);
  check(chain.size()==3,"chain rebuilt through the unstamped root");
}

void launchChoices(){
  std::ifstream input(fixtures/"normal.jsonl",std::ios::binary);
  std::stringstream buffer;buffer<<input.rdbuf();
  const Json log={{"messages",flights::parseJSONLBuffer(buffer.str())}};
  const auto recorded=application_flights::recordedLaunchChoices(log);
  check(recorded.model=="claude-sonnet-4-5-20250929","recorded model");
  check(recorded.permissionMode=="acceptEdits","recorded permission mode");
  application::Config config;
  config.profiles["sonnet"].model="claude-sonnet-4-5-20250929";
  config.profiles["local"].model="qwen3";
  check(application_flights::profileForModel(config,"claude-sonnet-4-5-20250929")=="sonnet",
    "wire model maps to its profile");
  check(application_flights::profileForModel(config,"local")=="local","profile id maps");
  check(!application_flights::profileForModel(config,"gone"),"unknown models are not restored");
  check(application_flights::restorablePermissionMode("plan"),"plan restores");
  check(!application_flights::restorablePermissionMode("bypassPermissions"),
    "bypass is never restored silently");
}

void generatedTitles(){
  using application_commands::parseGeneratedTitle;
  check(parseGeneratedTitle("{\"title\": \"Fix login button\"}")=="Fix login button","plain JSON");
  check(parseGeneratedTitle("```json\n{\"title\":\"Add OAuth\"}\n```")=="Add OAuth","fenced JSON");
  check(parseGeneratedTitle("Sure! {\"title\":\"Debug CI.\"} Hope that helps")=="Debug CI",
    "JSON inside prose, trailing period trimmed");
  check(parseGeneratedTitle("\"Refactor API client\"")=="Refactor API client","bare quoted title");
  check(!parseGeneratedTitle(""),"empty reply");
  check(!parseGeneratedTitle("{\"name\":\"x\"}"),"JSON without a title");
}
}

int main(){
  try{
    liteScanFixtures();
    liteScanGenerated();
    labels();
    matching();
    pickerRowParts();
    pickerAllProjectsSwitch();
    pickerFilter();
    pickerFlow();
    continueSkipsBadFiles();
    missingTimestampChain();
    launchChoices();
    generatedTitles();
    std::cout<<"flight listing: passed\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<"flight listing: "<<error.what()<<'\n';
    return 1;
  }
}
