#include "commands_indigenous/CoreCatalog.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool value,const char* message){
  if(!value)throw std::runtime_error(message);
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));return result;
}
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr failure;
  pending.then([&](const T* result,const std::exception_ptr& error){
    if(result)value=*result;
    failure=error;
  });
  for(int i=0;i<30&&!value&&!failure;++i)loop.runReady();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Command did not settle");
  return *value;
}
}
int main(){
  event_loop::Loop loop;
  std::optional<interactive::UiRequest> view;
  auto router=user_input::UiCommandRouter::create(loop,{
    [&](std::optional<interactive::UiRequest> next){view=std::move(next);},
    [](const std::string& failure){throw std::runtime_error(failure);},
    [](bool){},[](bool){}
  });
  auto clear=std::make_shared<commands_indigenous::ClearServices>(loop);
  std::vector<std::string> clearTrace;
  clear->flightEndHookTimeoutMs=[](){return 1500.0;};
  clear->flightEndHooks=[&](double){
    clearTrace.push_back("end");return ready(loop,event_loop::Unit{});};
  clear->flightStartHooks=[&]{
    clearTrace.push_back("start");return ready(loop,Json::array());};
  clear->setMessages=[&](Json){clearTrace.push_back("messages");};
  clear->feature=[](const std::string&){return false;};
  clear->clearFlightStashes=[&](const std::vector<std::string>&){
    clearTrace.push_back("caches");};
  clear->randomUUID=[](){return std::string("new");};
  clear->getOriginalCwd=[](){return std::string("/project");};
  clear->setCwd=[&](const std::string&){clearTrace.push_back("cwd");};
  clear->readFileState=std::make_shared<file_state_stash::FileStateStash>(100);
  clear->readFileState->set("/project/file",{"old",1});
  clear->clearAllPlanSlugs=[&]{clearTrace.push_back("plans");};
  clear->clearFlightMetadata=[&]{clearTrace.push_back("metadata");};
  clear->regenerateFlightId=[&](bool parent){
    check(parent,"clear lineage");clearTrace.push_back("session");};
  clear->resetFlightFilePointer=[&]{
    clearTrace.push_back("pointer");return ready(loop,event_loop::Unit{});};
  clear->getCurrentWorktreeFlight=[](){return std::optional<Json>{};};

  auto compact=std::make_shared<commands_indigenous::CompactServices>(loop);
  std::vector<std::string> compactTrace;
  compact->getMessages=[](){return Json::array({Json{{"type","user"}}});};
  compact->afterCompactBoundary=[](const Json& value){return value;};
  compact->tryFlightMemoryCompaction=[&](const Json&,
      const std::optional<std::string>&){
    compactTrace.push_back("memory");return ready(loop,std::optional<Json>{});};
  compact->reactiveOnly=[](){return false;};
  compact->microcompactMessages=[&](const Json& messages){
    compactTrace.push_back("micro");return ready(loop,Json{{"messages",messages}});};
  compact->stashSharingParams=[&](const Json&){
    compactTrace.push_back("params");return ready(loop,Json::object());};
  compact->compactConversation=[&](const Json&,const Json&,
      const std::string&){
    compactTrace.push_back("compact");return ready(loop,
      Json{{"userDisplayMessage","Summary"}});};
  compact->clearLastSummarizedMessageId=[&]{compactTrace.push_back("last");};
  compact->suppressCompactWarning=[&]{compactTrace.push_back("warning");};
  compact->clearUserContextStash=[&]{compactTrace.push_back("userCache");};
  compact->runPostCompactCleanup=[&]{compactTrace.push_back("cleanup");};
  compact->verbose=[](){return false;};
  compact->getUpgradeTip=[](){return std::string();};
  compact->getExpandShortcut=[](){return std::string("ctrl+o");};

  std::optional<std::string> modelName=std::string("standard");
  auto model=std::make_shared<commands_indigenous::ModelServices>(loop);
  model->getState=[&]{return commands_indigenous::ModelState{modelName,{}, {},false};};
  model->setModel=[&](std::optional<std::string> value){modelName=value;};
  model->defaultModelSetting=[](){return std::string("standard");};
  model->renderDefaultModelSetting=[](const std::string& value){return value;};
  model->renderModelName=[](const std::string& value){return value;};
  model->mainLoopModel=[&](){return modelName.value_or("standard");};
  model->isFastModeEnabled=[](){return false;};
  model->isExtendedContextMergeEnabled=[](){return false;};
  model->billedAsExtraUsage=[](const std::optional<std::string>&,bool,bool){
    return false;};
  model->isModelAllowed=[](const std::string&){return true;};
  model->premiumExtendedContextUnavailable=[](const std::string&){return false;};
  model->standardExtendedContextUnavailable=[](const std::string&){return false;};
  model->isKnownAlias=[](const std::string& value){return value=="fast";};
  model->validateModel=[&](const std::string&){
    return ready(loop,Json{{"valid",false},{"error","Unknown model"}});};
  model->immediate=[](){return false;};
  model->choices=[](){return std::vector<commands_indigenous::ModelChoice>{
    {"Fast",std::string("fast"),{}}};};

  auto resume=std::make_shared<commands_indigenous::ResumeServices>(loop);
  std::string resumedAt;
  resume->originalCwd=[](){return std::string("/project");};
  resume->currentFlightId=[](){return std::string("live");};
  resume->worktreePaths=[&](const std::string&){
    return ready(loop,std::vector<std::string>{"/project"});};
  resume->loadSameRepoLogs=[&](const std::vector<std::string>&){
    return ready(loop,Json::array({Json{{"id","live"}},
      Json{{"id","side"},{"isSidechain",true}},
      Json{{"id","old"},{"modified",1}}}));};
  resume->loadAllProjectsLogs=[&]{return ready(loop,Json::array());};
  resume->flightIdFromLog=[](const Json& value){
    return value.at("id").get<std::string>();};
  resume->logLabel=[](const Json& value){
    return value.at("id").get<std::string>();};
  resume->isLiteLog=[](const Json&){return false;};
  resume->loadFullLog=[&](const Json& value){return ready(loop,value);};
  resume->validateUuid=[](const std::string& value){
    return std::optional<std::string>(value);};
  resume->modifiedTimeMs=[](const Json& value){
    return value.value("modified",0.0);};
  resume->getLastFlightLog=[&](const std::string&){
    return ready(loop,std::optional<Json>{});};
  bool customTitles=false;
  resume->customTitleEnabled=[&](){return customTitles;};
  resume->searchByCustomTitle=[&](const std::string&,bool exact){
    check(exact,"exact title lookup");
    return ready(loop,Json::array({Json{{"id","titled"}}}));};
  resume->checkCrossProject=[](const Json&,bool,
      const std::vector<std::string>&){return commands_indigenous::CrossProject{};};
  resume->resume=[&](const std::string& id,const Json&,
      const std::string& entrypoint){
    resumedAt=id+":"+entrypoint;return ready(loop,event_loop::Unit{});};

  commands_indigenous::CoreCommandServices services{
    router,clear,compact,{},model,resume,{}};
  auto commands=commands_indigenous::buildCoreCommands(services);
  check(commands.size()==5,"five core commands");
  check(commands[0].descriptor.at("name")=="help","help registration");
  check(commands[1].descriptor.at("aliases").at(0)=="reset",
    "clear aliases");
  check(commands[4].descriptor.at("aliases").at(0)=="continue",
    "resume alias");
  auto cleared=await(loop,await(loop,commands[1].loadLocal())(""));
  check(cleared==Json{{"type","text"},{"value",""}},"clear result");
  check(clear->readFileState->size()==0,"read cache cleared");
  check(clearTrace==std::vector<std::string>({"end","messages","caches",
    "cwd","plans","metadata","session","pointer","start"}),
    "clear order");
  auto compacted=await(loop,await(loop,commands[2].loadLocal())(""));
  check(compacted.at("type")=="compact","compact result");
  check(compactTrace==std::vector<std::string>({"memory","micro","params",
    "compact","last","warning","userCache","cleanup"}),
    "compact order");
  auto information=await(loop,commands[3].invokeUi("status"));
  check(information.result=="Current model: standard","model status");
  auto changed=await(loop,commands[3].invokeUi("fast"));
  check(changed.result=="Set model to fast"&&modelName=="fast",
    "model inline alias");
  auto invalid=await(loop,commands[3].invokeUi("missing"));
  check(invalid.display=="system"&&invalid.result=="Unknown model",
    "model validation result");
  auto direct=await(loop,commands[4].invokeUi("old"));
  check(direct.display=="skip"&&resumedAt=="old:slash_command_session_id",
    "resume exact session");
  customTitles=true;
  auto byTitle=await(loop,commands[4].invokeUi("named"));
  check(byTitle.display=="skip"&&resumedAt=="titled:slash_command_title",
    "resume title");
  auto picked=commands[4].invokeUi("");
  for(int i=0;i<10&&(!view||view->id!="resume");++i)loop.runReady();
  check(view&&view->id=="resume"&&view->choices.size()==2,
    "resume filters current and sidechain");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  const auto resumed=await(loop,picked);
  check(resumed.display=="skip"&&resumedAt=="old:slash_command_picker",
    "resume picker completion");
  auto help=commands[0].invokeUi("");
  check(view&&view->id=="help","help visible");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
  const auto dismissed=await(loop,help);
  check(dismissed.display=="system"&&
    dismissed.result=="Help dialog dismissed","help close");
  std::cout<<"core commands native flow passed\n";
}
