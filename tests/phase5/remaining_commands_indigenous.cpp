#include "commands_indigenous/RemainingCatalog.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
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
  auto router=user_input::UiCommandRouter::create(loop,{
    [](std::optional<interactive::UiRequest>){},
    [](const std::string& message){throw std::runtime_error(message);},
    [](bool){},[](bool){}
  });
  std::vector<std::string> trace;
  auto rename=std::make_shared<commands_indigenous::RenameServices>(loop);
  rename->isPartner=[](){return false;};
  rename->flightId=[](){return std::string("session");};
  rename->transcriptPath=[](){return std::string("/tmp/session.jsonl");};
  rename->saveCustomTitle=[&](const std::string&,const std::string&,
      const std::string&){trace.push_back("title");
    return ready(loop,event_loop::Unit{});};
  rename->bridgeFlightId=[](){return std::optional<std::string>("bridge");};
  rename->scheduleBridgeTitleUpdate=[&](const std::string&,
      const std::string&){trace.push_back("bridge");};
  rename->saveBatName=[&](const std::string&,const std::string&,
      const std::string&){trace.push_back("agent");
    return ready(loop,event_loop::Unit{});};
  rename->setStandaloneBatName=[&](const std::string&){
    trace.push_back("state");};
  auto cost=std::make_shared<commands_indigenous::CostServices>(loop);
  cost->isSubscriber=[](){return false;};
  cost->formatTotalCost=[](){return std::string("$0.42");};
  auto rewind=std::make_shared<commands_indigenous::RewindServices>(loop);
  rewind->openMessageSelector=[&]{trace.push_back("selector");};
  auto exit=std::make_shared<commands_indigenous::ExitServices>(loop);
  exit->backgroundFlightsEnabled=[](){return true;};
  exit->isBackgroundFlight=[](){return true;};
  exit->hasCurrentWorktree=[](){return false;};
  exit->detachBackgroundFlight=[&]{trace.push_back("detach");};
  auto monster=std::make_shared<commands_indigenous::MonsterServices>();
  monster->roller=std::make_shared<monster::Roller>(monster::Services{
    [](){return Json{{"userID","u"},
      {"monster",Json{{"name","Milo"}}}};},{}
  });
  auto pages=std::make_shared<commands_indigenous::PageServices>();
  auto context=std::make_shared<bats::BatContext>();
  context->getAppState=[](){return Json{{"toolPermissionContext",
    Json::object()}};};
  pages->getContext=[&]{return context;};
  pages->getGadgets=[](const Json&){return gadgets::Gadgets{};};
  pages->appendPermissionRetryMessage=[&](const Json&){
    trace.push_back("retry");};
  pages->openPage=[&](commands_indigenous::PageRequest request,
      interactive::CommandUi::Done done,interactive::CommandUi::Ready,
      interactive::CommandUi::Error){
    check(request.context==context,"typed context identity");
    if(request.kind==commands_indigenous::PageKind::Permissions)
      request.retryDeniedCommands(Json::array({"Bash"}));
    interactive::CommandCompletion completed;
    completed.result="page";
    done(std::move(completed));
  };
  auto commands=commands_indigenous::buildRemainingCommands({
    router,rename,cost,exit,rewind,monster,pages});
  check(commands.size()==10,"remaining command catalog");
  auto renamed=await(loop,commands.at(0).invokeUi("New name"));
  check(renamed.result=="Flight renamed to: New name"&&
    trace==std::vector<std::string>({"title","bridge","agent","state"}),
    "rename order");
  auto status=await(loop,commands.at(1).invokeUi(""));
  check(status.result=="page","status page");
  auto permission=await(loop,commands.at(5).invokeUi(""));
  check(permission.result=="page"&&trace.back()=="retry",
    "permission retry callback");
  auto costResult=await(loop,await(loop,commands.at(6).loadLocal())(""));
  check(costResult.at("value")=="$0.42","cost");
  auto rewindResult=await(loop,await(loop,commands.at(7).loadLocal())(""));
  check(rewindResult.at("type")=="skip"&&trace.back()=="selector",
    "rewind selector");
  auto detached=await(loop,commands.at(8).invokeUi(""));
  check(!detached.result&&trace.back()=="detach","background detach");
  auto monsterResult=commands.at(9).invokeUi("");
  check(router->ui()->visibleUi()&&
    router->ui()->visibleUi()->id=="monster","monster view");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
  check(await(loop,monsterResult).display=="skip","monster close");
  std::cout<<"remaining native command flow passed\n";
}
