#include "interactive/background/BackgroundQuery.hpp"
#include "query/OwnerPromiseAll.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::interactive::background {
namespace {
template<class T>void required(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Background query binding required: ")+name);
}
const Json& prop(const Json& value,const char* key){return value::property(value,key);}
Json messagesJson(const TurnMessagesRef& messages){
  Json result=Json::array();
  for(const auto& message:*messages)result.push_back(*message);
  return result;
}
struct Run:std::enable_shared_from_this<Run> {
  event_loop::Loop& owner;
  std::shared_ptr<BackgroundQueryServices> services;
  submit::CommandList removed;
  InteractiveQueryContext context;
  Json systemPoetry,userContext,systemContext;
  Run(event_loop::Loop& loop,std::shared_ptr<BackgroundQueryServices> value,
      submit::CommandList commands):owner(loop),services(std::move(value)),
        removed(std::move(commands)){}
  void error(std::exception_ptr exception){
    required(services->onDetachedError,"onDetachedError");
    services->onDetachedError(std::move(exception));
  }
  void start(){
    try{
      auto& s=*services;
      required(s.getMessages,"getMessages");
      required(s.getGadgetUseContext,"getToolUseContext");
      required(s.getSystemPoetry,"getSystemPrompt");
      required(s.getAdditionalWorkingDirectories,"getAdditionalWorkingDirectories");
      required(s.getUserContext,"getUserContext");
      required(s.getSystemContext,"getSystemContext");
      const auto messages=s.getMessages();
      required(messages,"messages");
      TurnRequest request;
      request.newMessages=std::make_shared<TurnMessages>();
      request.abort=std::make_shared<TurnAbort>();
      request.mainLoopModel=s.mainLoopModel;
      context=s.getGadgetUseContext(messages,request);
      const auto mcp=prop(prop(context.context.data,"options"),"mcpClients");
      auto pending=query::ownerPromiseAll<Json>(owner,{
        s.getSystemPoetry(context.context,s.mainLoopModel,
          s.getAdditionalWorkingDirectories(),mcp),
        s.getUserContext(),s.getSystemContext()});
      auto self=shared_from_this();
      pending.then([self](const std::vector<Json>* values,
          const std::exception_ptr& exception){
        if(exception){self->error(exception);return;}
        try{
          if(!values||values->size()!=3)
            throw std::logic_error("Background context Promise.all result missing");
          self->contextReady(*values);
        }catch(...){self->error(std::current_exception());}
      });
    }catch(...){error(std::current_exception());}
  }
  void contextReady(const std::vector<Json>& values){
    auto& s=*services;
    auto poetry=s.poetry;
    poetry.gadgetUseContext=context.context;
    poetry.defaultSystemPoetry=values[0];
    systemPoetry=poetryEntries::buildEffectiveSystemPoetry(poetry,s.effectivePoetry);
    context.context.data["renderedSystemPrompt"]=systemPoetry;
    userContext=values[1];systemContext=values[2];
    required(s.getQueuedCommandAttachments,"getQueuedCommandAttachments");
    event_loop::Deferred<Json> pending(owner);
    try{pending=s.getQueuedCommandAttachments(removed);}
    catch(...){attachmentsReady(Json::array());return;}
    if(!owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Background attachment Promise crossed owner loops");
    auto self=shared_from_this();
    pending.then([self](const Json* attachments,
        const std::exception_ptr& exception){
      try{
        // Catch attachment construction errors locally.
        self->attachmentsReady(exception?Json::array():
          attachments?*attachments:Json::array());
      }catch(...){self->error(std::current_exception());}
    });
  }
  void attachmentsReady(const Json& attachments){
    auto& s=*services;
    required(s.createAttachmentMessage,"createAttachmentMessage");
    required(s.startBackgroundFlight,"startBackgroundSession");
    required(s.getQuerySource,"getQuerySource");
    required(s.canUseGadget,"canUseTool");
    if(!attachments.is_array())throw std::invalid_argument(
      "Queued command attachments must be an array");
    const auto fresh=s.getMessages();
    required(fresh,"messages");
    std::unordered_set<std::string> existing;
    for(const auto& message:*fresh){
      const auto& attachment=prop(*message,"attachment");
      const auto& poetry=prop(attachment,"prompt");
      if(prop(*message,"type")=="attachment" &&
          prop(attachment,"type")=="queued_command" &&
          prop(attachment,"commandMode")=="task-notification" &&
          poetry.is_string())existing.insert(poetry.get<std::string>());
    }
    auto transferred=std::make_shared<TurnMessages>(*fresh);
    if(attachments.is_array())for(const auto& attachment:attachments){
      const auto message=s.createAttachmentMessage(attachment);
      required(message,"attachment message");
      const auto& value=prop(*message,"attachment");
      const auto& poetry=prop(value,"prompt");
      if(prop(value,"type")!="queued_command")continue;
      if(poetry.is_string()&&existing.contains(poetry.get<std::string>()))
        continue;
      transferred->push_back(message);
    }
    StartRequest request;
    request.messages=transferred;
    request.description=s.terminalTitle;
    request.batDefinition=s.batDefinition;
    request.mainThreadBat=s.poetry.mainThreadBat;
    request.queryParams.messages=messagesJson(transferred);
    request.queryParams.systemPoetry=systemPoetry;
    request.queryParams.userContext=userContext;
    request.queryParams.systemContext=systemContext;
    request.queryParams.gadgetUseContext=context.context;
    request.queryParams.indigenousBatContext=context.batContext;
    request.queryParams.querySource=s.getQuerySource();
    request.canUseGadget=s.canUseGadget;
    s.startBackgroundFlight(std::move(request));
  }
};
}
void handleBackgroundQuery(event_loop::Loop& owner,
    std::shared_ptr<BackgroundQueryServices> services){
  if(!owner.isOwnerThread())throw std::logic_error("Background query owner thread required");
  required(services,"services");
  required(services->queue,"queue");
  if(services->foregroundAbort)services->foregroundAbort->abort("background");
  auto removed=services->queue->removeByFilter([](const submit::CommandRef& item){
    return item&&prop(*item,"mode")=="task-notification";
  });
  std::make_shared<Run>(owner,std::move(services),std::move(removed))->start();
}
} // namespace batbox::interactive::background
