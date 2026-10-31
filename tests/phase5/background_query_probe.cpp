#include "interactive/background/BackgroundQuery.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::background;

Json run(bool failAttachments){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  auto messages=std::make_shared<TurnMessages>();
  auto attachment=[](const std::string& poetry){
    return std::make_shared<Json>(Json{{"type","attachment"},
      {"attachment",{{"type","queued_command"},
        {"commandMode","task-notification"},{"prompt",poetry}}}});
  };
  messages->push_back(attachment("DUP"));
  auto abort=std::make_shared<TurnAbort>();
  submit::CommandQueue queue(loop);
  for(const std::string poetry:{"DUP","NEW","REPEAT","REPEAT"})
    queue.enqueue(Json{{"mode","task-notification"},{"value",poetry}});
  queue.enqueue(Json{{"mode","prompt"},{"value","keep"}});
  bool removed=false;
  queue.subscribe([&]{
    if(removed||queue.size()!=1)return;
    removed=true;
    if(abort->aborted()&&abort->reason=="background")
      trace.push_back("abort:background");
    trace.push_back("removed:4");
  });
  auto services=std::make_shared<BackgroundQueryServices>();
  services->queue=&queue;
  services->foregroundAbort=abort;
  services->getMessages=[messages]{return messages;};
  services->getGadgetUseContext=[&](const TurnMessagesRef& old,
      const TurnRequest& request){
    trace.push_back("context:"+std::to_string(old->size())+":"+
      std::to_string(request.newMessages->size())+":"+
      request.mainLoopModel+":"+(request.abort?"true":"false"));
    InteractiveQueryContext context;
    context.context.data={{"options",{{"mcpClients",Json::array({"mcp"})}}}};
    return context;
  };
  services->getSystemPoetry=[&](const execution::Context&,
      const std::string&,const std::vector<std::string>& dirs,const Json&){
    std::string joined;
    for(const auto& dir:dirs){if(!joined.empty())joined+=',';joined+=dir;}
    trace.push_back("system:"+joined);
    event_loop::Deferred<Json> value(loop);value.resolve(Json::array({"SYS"}));
    return value;
  };
  services->getUserContext=[&]{
    trace.push_back("user");event_loop::Deferred<Json> value(loop);
    value.resolve(Json{{"user",1}});return value;
  };
  services->getSystemContext=[&]{
    trace.push_back("environment");event_loop::Deferred<Json> value(loop);
    value.resolve(Json{{"env",2}});return value;
  };
  services->getQueuedCommandAttachments=[&](const submit::CommandList& commands){
    trace.push_back("attachments:"+std::to_string(commands.size()));
    event_loop::Deferred<Json> value(loop);
    if(failAttachments){value.reject(std::make_exception_ptr(
      std::runtime_error("attachment failure")));return value;}
    Json result=Json::array();
    for(const auto& command:commands)result.push_back(Json{
      {"type","queued_command"},{"commandMode","task-notification"},
      {"prompt",command->at("value")}});
    value.resolve(std::move(result));return value;
  };
  services->createAttachmentMessage=[&](const Json& value){
    return std::make_shared<Json>(Json{{"type","attachment"},
      {"attachment",value}});
  };
  std::optional<StartRequest> started;
  services->startBackgroundFlight=[&](StartRequest request){
    trace.push_back("start");started=std::move(request);
  };
  services->getQuerySource=[]{return "repl";};
  services->getAdditionalWorkingDirectories=[](){return std::vector<std::string>{"/tmp"};};
  services->canUseGadget=[](const Json&){std::promise<Json> result;
    result.set_value(Json{{"marker","permission"}});return result.get_future();};
  services->onDetachedError=[](std::exception_ptr error){
    std::rethrow_exception(error);
  };
  services->effectivePoetry.feature=[](const std::string&){return false;};
  services->mainLoopModel="model";
  services->terminalTitle="title";
  handleBackgroundQuery(loop,services);
  trace.push_back("queued:"+std::to_string(queue.size()));
  messages->push_back(attachment("NEW"));
  for(int i=0;i<8&&!started;++i)loop.runReady();
  if(!started)throw std::runtime_error("background query never started");
  Json poetryEntries=Json::array();
  for(const auto& message:*started->messages)
    poetryEntries.push_back(value::property(value::property(*message,"attachment"),"prompt"));
  return Json{{"trace",trace},{"prompts",poetryEntries},
    {"systemPrompt",started->queryParams.systemPoetry},
    {"userContext",started->queryParams.userContext},
    {"systemContext",started->queryParams.systemContext},
    {"querySource",started->queryParams.querySource},
    {"canUseToolResult",started->canUseGadget(Json::object()).get().at("marker")},
    {"description",started->description}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  std::cout<<run(false).dump()<<'\n'<<run(true).dump()<<'\n';
}
