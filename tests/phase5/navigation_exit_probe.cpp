#include "interactive/navigation/ExitFlow.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::navigation;
Json run(const std::string& kind){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  ExitServices services(loop);
  services.publishExiting=[&](bool value){
    trace.push_back(std::string("exiting:")+(value?"true":"false"));
  };
  services.publishExitFlow=[&](std::optional<UiRequest> view,bool undefined){
    trace.push_back(std::string("flow:")+
      (view?"view":(undefined?"undefined":"null")));
  };
  services.feature=[&](const std::string& name){
    return name=="BG_SESSIONS"&&kind=="background";
  };
  services.isBgFlight=[&]{return kind=="background";};
  services.detachTmuxClient=[&]{trace.push_back("detach:tmux:detach-client");};
  services.getCurrentWorktreeFlight=[&]{
    return kind=="worktree"?Json{{"path","w"}}:Json();
  };
  services.makeWorktreeExitRequest=[](){
    UiRequest request;request.kind=UiRequest::Kind::Confirm;
    request.id="worktree";return request;
  };
  services.loadExit=[&]{
    trace.push_back("load");
    event_loop::Deferred<ExitServices::ExitCall> loaded(loop);
    if(kind=="failure")loaded.reject(std::make_exception_ptr(
      std::runtime_error("load failed")));
    else loaded.resolve([&](std::function<void()>){
      trace.push_back("call");
      event_loop::Deferred<ExitServices::Result> result(loop);
      ExitServices::Result value;
      if(kind=="view"){
        value.isNull=false;
        UiRequest request;request.id="loaded";value.view=request;
      }else if(kind=="undefined")value.isNull=false;
      result.resolve(value);return result;
    });
    return loaded;
  };
  auto controller=std::make_shared<ExitFlow>(std::move(services));
  auto pending=controller->handleExit();
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)try{std::rethrow_exception(error);}
      catch(const std::exception& value){
        trace.push_back(std::string("rejected:")+value.what());
      }
  });
  for(int i=0;i<6;++i)loop.runReady();
  if(kind=="worktree")controller->cancelFlow();
  return Json{{"trace",trace},{"isExiting",controller->isExiting()},
    {"flow",controller->flow()?Json("view"):
      (controller->flowUndefined()?Json("undefined"):Json())}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  for(const auto& kind:{"background","worktree","null","view",
      "undefined","failure"})std::cout<<run(kind).dump()<<'\n';
}
