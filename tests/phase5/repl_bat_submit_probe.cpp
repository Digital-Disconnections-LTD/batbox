#include "interactive/lifecycle/BatSubmit.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
Json run(const std::string& kind){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  auto messages=std::make_shared<TurnMessages>();
  auto services=std::make_shared<BatSubmitServices>(loop);
  services->createUserMessage=[&](const std::string& text){
    trace.push_back("create:"+text);
    return std::make_shared<Json>(Json{{"content",text}});
  };
  services->appendMessageToLocalBat=[&](const std::string& id,TurnMessageRef message){
    trace.push_back("append:"+id+":"+(*message)["content"].get<std::string>());
  };
  services->queuePendingMessage=[&](const std::string& id,const std::string& text){
    trace.push_back("queue:"+id+":"+text);
  };
  services->injectUserMessageToPartner=[&](const std::string& id,
      const std::string& text){trace.push_back("inject:"+id+":"+text);};
  services->getMessages=[messages]{return messages;};
  services->mainLoopModel=[]{return "m";};
  services->getGadgetUseContext=[&](TurnMessagesRef current,
      const std::vector<std::string>& dirs,const std::shared_ptr<TurnAbort>& abort,
      const std::string& model){
    trace.push_back(std::string("context:")+
      (current==messages?"true":"false")+":"+std::to_string(dirs.size())+
      ":"+model+":"+(abort?"true":"false"));
    return std::make_shared<context::GadgetUseContext>();
  };
  services->canUseGadget=[](const Json&){
    std::promise<Json> result;result.set_value(true);return result.get_future();
  };
  services->resumeBatBackground=[&](BatResumeRequest request){
    trace.push_back("resume:"+request.batId+":"+request.poetry+":"+
      (request.gadgetUseContext?"ctx":"none")+":"+
      (request.canUseGadget?"true":"false"));
    event_loop::Deferred<event_loop::Unit> done(loop);
    if(kind=="failure")done.reject(std::make_exception_ptr(
      std::runtime_error("oops")));
    else done.resolve({});
    return done;
  };
  services->logForDebugging=[&](std::string message){
    trace.push_back("log:"+message);
  };
  services->errorMessage=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string(value.what());
    }
  };
  services->addLowPriorityNotification=[&](const std::string& key,
      const std::string&){trace.push_back("notice:"+key+":low");};
  services->setInputValue=[&](std::string value){trace.push_back("input:"+value);};
  BatSubmitHelpers helpers;
  helpers.setCursorOffset=[&](std::size_t value){
    trace.push_back("cursor:"+std::to_string(value));
  };
  helpers.clearBuffer=[&]{trace.push_back("clear");};
  onBatSubmit("hello",BatSubmitMission{"a1",
    kind=="running"?"running":"idle",kind!="teammate"},
    helpers,services);
  loop.runReady();
  return Json{{"trace",trace}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  for(const auto& kind:{"running","paused","failure","teammate"})
    std::cout<<run(kind).dump()<<'\n';
}
