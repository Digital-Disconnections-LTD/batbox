#include "application_model/Internal.hpp"
#include "query/SimpleQueriesBinding.hpp"
#include "runtime/Platform.hpp"
#include <future>
#include <stdexcept>
#include <utility>

namespace batbox::application_model {
namespace {
template<class Callback>void require(const Callback& callback,const char* name){
  if(!callback)throw std::invalid_argument(
    std::string("Local model import required: ")+name);
}
void validate(const Config& config,const Imports& imports){
  if(config.client.baseUrl.empty()||config.client.messagesPath.empty())
    throw std::invalid_argument("Local model URL and messages path required");
  if(config.mainModel.empty()||config.smallModel.empty())
    throw std::invalid_argument("Main and small model IDs required");
  for(const auto& id:{config.mainModel,config.smallModel,config.visionModel}){
    if(id.empty())continue;
    const auto found=config.profiles.find(id);
    if(found==config.profiles.end()||found->second.id!=id||
       found->second.contextTokens<=0||found->second.maxOutputTokens<=0)
      throw std::invalid_argument("Missing local model profile: "+id);
    for(const auto* key:{"inputTokens","outputTokens",
        "promptCacheReadTokens","promptCacheWriteTokens","webSearchRequests"})
      if(!found->second.costs.is_object()||
         !found->second.costs.contains(key)||
         !found->second.costs.at(key).is_number())
        throw std::invalid_argument("Local model cost required: "+id+" "+key);
  }
  for(const auto& [name,profile]:config.profiles){
    if(profile.id!=name||profile.contextTokens<=0||
       profile.maxOutputTokens<=0)
      throw std::invalid_argument("Invalid local model profile: "+name);
    if(profile.client&&(profile.client->baseUrl.empty()||
       profile.client->messagesPath.empty()))
      throw std::invalid_argument("Missing Messages path for profile: "+name);
    if(profile.authHeader!="authorization"&&profile.authHeader!="x-api-key"&&
       profile.authHeader!="none")
      throw std::invalid_argument("Invalid authentication header for profile: "+name);
    for(const auto* key:{"inputTokens","outputTokens",
        "promptCacheReadTokens","promptCacheWriteTokens","webSearchRequests"})
      if(!profile.costs.is_object()||!profile.costs.contains(key)||
         !profile.costs.at(key).is_number())
        throw std::invalid_argument("Local model cost required: "+name+" "+key);
  }
  if(config.gadgetSearch||config.stashedMicrocompact)
    throw std::invalid_argument(
      "Local model tool search and cache editing need configured imports");
  if(config.timeBasedMicrocompact){
    require(imports.clearCompactWarningSuppression,
      "clearCompactWarningSuppression");
    require(imports.suppressCompactWarning,"suppressCompactWarning");
    if(config.features.contains("PROMPT_CACHE_BREAK_DETECTION"))
      require(imports.notifyStashDeletion,"notifyCacheDeletion");
  }
  if(config.fastMode){
    require(imports.disableFastModeInSettings,"disableFastModeInSettings");
    require(imports.fastModeOverageRejection,"fastModeOverageRejection");
  }
  if(config.fastMode&&config.profiles.at(config.mainModel).fastModeSupported){
    const auto& rates=config.profiles.at(config.mainModel).fastCosts;
    for(const auto* key:{"inputTokens","outputTokens",
        "promptCacheReadTokens","promptCacheWriteTokens","webSearchRequests"})
      if(!rates.is_object()||!rates.contains(key)||!rates.at(key).is_number())
        throw std::invalid_argument("Local fast-mode cost required: "+
          config.mainModel+" "+key);
  }
  require(imports.authToken,"authToken");
  require(imports.invalidateAuthToken,"invalidateAuthToken");
  require(imports.debug,"debug");
  require(imports.diagnostic,"diagnostic");
  require(imports.normalizeAttachmentForAPI,"normalizeAttachmentForAPI");
  require(imports.normalizeGadgetInputForAPI,"normalizeToolInputForAPI");
  require(imports.gadgetSchemaAsync,"toolSchemaAsync");
  require(imports.gadgetSchemaSync,"toolSchemaSync");
}
}
LocalModelService::LocalModelService(event_loop::Loop& owner,Config config,
    Imports imports):owner_(owner),config_(std::move(config)),
    imports_(std::move(imports)){
  validate(config_,imports_);
}
ConfigurationSnapshot LocalModelService::configurationSnapshot()const{
  std::lock_guard lock(mutex_);
  return {config_,runtimeAuthTokens_};
}
LocalState LocalModelService::snapshot()const{
  std::lock_guard lock(mutex_);return state_;
}
std::optional<ModelProfile> LocalModelService::findProfile(const std::string& id)const{
  std::lock_guard lock(mutex_);
  const auto found=config_.profiles.find(id);
  if(found==config_.profiles.end())return std::nullopt;
  return found->second;
}
void LocalModelService::upsertRuntimeProfile(ModelProfile profile,std::string authToken){
  if(!owner_.isOwnerThread())throw std::logic_error("Runtime model changes require owner loop");
  if(profile.id.empty()||profile.wireModel.empty()||profile.contextTokens<=0||
      profile.maxOutputTokens<=0||!profile.client||
      profile.client->baseUrl.empty()||profile.client->messagesPath.empty())
    throw std::invalid_argument("Incomplete runtime model profile");
  if(profile.authHeader!="authorization"&&profile.authHeader!="x-api-key"&&
      profile.authHeader!="none")
    throw std::invalid_argument("Invalid runtime profile authentication");
  std::lock_guard lock(mutex_);
  runtimeAuthTokens_.insert_or_assign(profile.id,std::move(authToken));
  config_.profiles.insert_or_assign(profile.id,std::move(profile));
}
void LocalModelService::removeRuntimeProfile(const std::string& id){
  if(!owner_.isOwnerThread())throw std::logic_error("Runtime model changes require owner loop");
  std::lock_guard lock(mutex_);
  if(!config_.profiles.contains(id))return;
  if(config_.profiles.size()==1)
    throw std::invalid_argument("Cannot remove the last configured model");
  runtimeAuthTokens_.erase(id);config_.profiles.erase(id);
  const auto& fallback=config_.profiles.begin()->first;
  if(config_.mainModel==id)config_.mainModel=fallback;
  if(config_.smallModel==id)config_.smallModel=fallback;
  if(config_.visionModel==id)config_.visionModel=fallback;
}
model::QueryServices LocalModelService::makeQueryServices(
    const gadgets::Gadgets& gadgets,const Json& options,
    gadgets::PoetryContext poetry){
  auto services=LocalServiceBuilder(*this,gadgets,options,
    std::move(poetry)).makeServices();
  const auto keepAlive=shared_from_this();
  const auto create=services.createClient;
  services.createClient=[keepAlive,create]{return create();};
  return services;
}
query::AsyncModelCursor LocalModelService::makeCursor(
    const query::ModelRequest& request,
    const std::function<void()>& onStreamingFallback){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Local model cursor requires owner loop");
  model::PreflightInputs input;
  input.messages=request.messages;
  input.systemPoetry=request.systemPoetry;
  input.thinkingConfig=request.thinkingConfig;
  input.options=request.options;
  if(!input.options.is_object())input.options=Json::object();
  if(!input.options.contains("model")||input.options.at("model").is_null())
    input.options["model"]=config_.mainModel;
  gadgets::PoetryContext poetry;
  if(request.getGadgetPermissionContext){
    poetry.getGadgetPermissionContext=[get=request.getGadgetPermissionContext]{
      std::promise<Json> result;
      try{result.set_value(get());}
      catch(...){result.set_exception(std::current_exception());}
      return result.get_future();
    };
  }
  const auto& bats=value::property(input.options,"agents");
  if(!bats.is_null())poetry.bats=bats;
  poetry.allowedBatTypes=value::property(input.options,"allowedAgentTypes");
  LocalServiceBuilder builder(*this,request.gadgets,input.options,poetry);
  input.gadgets=builder.gadgetMetadata();
  auto services=makeQueryServices(request.gadgets,input.options,
    std::move(poetry));
  services.signal=request.signal;
  auto awaits=builder.makeOwnerPreflight();
  return query::makeOwnerSegmentQueryModelCursor(owner_,std::move(input),
    std::move(services),std::move(awaits),onStreamingFallback);
}
event_loop::Deferred<model::MessageRef> LocalModelService::querySmallFast(
    model::SimpleQueryInputs input){
  auto self=shared_from_this();
  model::SimpleQueryServices services;
  services.messageServices=LocalServiceBuilder(*this,{},input.options).makeMessages();
  services.getSmallFastModel=[self]{return self->config_.smallModel;};
  services.queryWithoutStreaming=[self](model::SimpleQueryRequest request){
    auto input=std::move(request.input);
    LocalServiceBuilder builder(*self,{},input.options);
    auto services=self->makeQueryServices({},input.options);
    services.signal=request.signal;
    auto cursor=query::makeOwnerSegmentQueryModelCursor(self->owner_,
      std::move(input),std::move(services),builder.makeOwnerPreflight());
    return query::collectLastAssistantMessage(self->owner_,std::move(cursor),
      request.signal);
  };
  return model::querySmallFast(owner_,std::move(input),std::move(services));
}
event_loop::Deferred<model::MessageRef> LocalModelService::queryWithModel(
    model::SimpleQueryInputs input){
  auto self=shared_from_this();
  model::SimpleQueryServices services;
  services.messageServices=LocalServiceBuilder(*this,{},input.options).makeMessages();
  services.queryWithoutStreaming=[self](model::SimpleQueryRequest request){
    auto input=std::move(request.input);
    LocalServiceBuilder builder(*self,{},input.options);
    auto services=self->makeQueryServices({},input.options);
    services.signal=request.signal;
    auto cursor=query::makeOwnerSegmentQueryModelCursor(self->owner_,
      std::move(input),std::move(services),builder.makeOwnerPreflight());
    return query::collectLastAssistantMessage(self->owner_,std::move(cursor),
      request.signal);
  };
  return model::queryWithModel(owner_,std::move(input),std::move(services));
}
Json LocalModelService::gadgetMetadata(const gadgets::Gadgets& activeGadgets){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Hook tool metadata requires owner loop");
  return LocalServiceBuilder(*this,activeGadgets,Json::object()).gadgetMetadata();
}
event_loop::Deferred<model::MessageRef> LocalModelService::queryPoetryHook(
    model::SimpleQueryRequest request,gadgets::Gadgets activeGadgets){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Prompt hook query requires owner loop");
  event_loop::Deferred<model::MessageRef> output(owner_);
  if(!request.getGadgetPermissionContext){
    output.reject(std::make_exception_ptr(std::invalid_argument(
      "Prompt hook permission context is required")));
    return output;
  }
  auto self=shared_from_this();
  request.getGadgetPermissionContext().then(
    [self,output,request=std::move(request),
        activeGadgets=std::move(activeGadgets)](
        const Json* permission,const std::exception_ptr& error)mutable{
      if(error){output.reject(error);return;}
      try{
        if(!permission)throw std::runtime_error(
          "Prompt hook permission context is missing");
        query::ModelRequest modelRequest;
        modelRequest.messages=std::move(request.input.messages);
        modelRequest.systemPoetry=std::move(request.input.systemPoetry);
        modelRequest.thinkingConfig=std::move(request.input.thinkingConfig);
        modelRequest.gadgets=std::move(activeGadgets);
        modelRequest.signal=request.signal;
        modelRequest.options=std::move(request.input.options);
        modelRequest.getGadgetPermissionContext=[state=*permission]{return state;};
        auto cursor=self->makeCursor(modelRequest);
        query::collectLastAssistantMessage(self->owner_,std::move(cursor),
          request.signal).then([output](const model::MessageRef* message,
              const std::exception_ptr& failure){
            if(failure)output.reject(failure);
            else output.resolve(*message);
          });
      }catch(...){output.reject(std::current_exception());}
    });
  return output;
}
} // namespace batbox::application_model
