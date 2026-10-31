#include "application_model/Internal.hpp"
#include "runtime/Platform.hpp"
#include <algorithm>
#include <cctype>
#include <utility>

namespace batbox::application_model {
model::QueryServices LocalServiceBuilder::makeServices(){
  model::QueryServices service;
  auto& local=host;
  service.environment=runtime.config.environment;
  service.stashedMicrocompactFeature=runtime.config.stashedMicrocompact;
  service.disableFallbackFeature=runtime.config.disableStreamingFallback;
  service.logForDebugging=[&local](const auto& value,const auto& level){
    local.imports_.debug(level,value);
  };
  service.logForDiagnosticsNoPII=[&local](const auto& level,
      const auto& value){local.imports_.diagnostic(level,value);};
  service.queryCheckpoint=[&local](const auto& value){
    local.imports_.diagnostic("checkpoint",value);
  };
  service.headlessProfilerCheckpoint=[&local](const auto& value){
    local.imports_.diagnostic("profile",value);
  };
  service.endQueryProfile=[&local]{
    local.imports_.diagnostic("profile","end");
  };
  service.startFlightActivity=[&local](const auto& reason){
    {
      std::lock_guard lock(local.mutex_);
      ++local.state_.activeRequests;
    }
    local.imports_.diagnostic("activity_start",reason);
  };
  service.stopFlightActivity=[&local](const auto& reason){
    {
      std::lock_guard lock(local.mutex_);
      if(local.state_.activeRequests>0)--local.state_.activeRequests;
    }
    local.imports_.diagnostic("activity_stop",reason);
  };
  service.addToTotalDurationState=[&local](double includingRetries,
      double finalAttempt){
    std::lock_guard lock(local.mutex_);
    local.state_.totalAPIDuration+=includingRetries;
    local.state_.totalAPIDurationWithoutRetries+=finalAttempt;
  };
  service.postStream.poetryStashBreakFeature=[]{return false;};
  service.postStream.extractQuotaStatusFromHeaders=[&local](const Json& headers){
    std::lock_guard lock(local.mutex_);local.state_.lastQuotaHeaders=headers;
  };
  service.preflight=makePreflight();
  service.builder=makeBuilder();
  service.wire=runtime.config.requestWire;
  service.headers=runtime.config.builderHeaders;
  service.retry=makeRetry();
  service.reducer=makeReducer();
  const auto selected=value::property(options,"model");
  const std::string modelName=selected.is_string()
    ?selected.get<std::string>():runtime.config.mainModel;
  const auto found=runtime.config.profiles.find(modelName);
  if(found==runtime.config.profiles.end())
    throw std::invalid_argument("Unconfigured local model: "+modelName);
  const auto profile=found->second;
  if(const auto baseSegment=local.imports_.buildParamsSegment){
    std::map<std::string,bool> sendDisabledByProfile;
    for(const auto& [id,configured]:runtime.config.profiles)
      sendDisabledByProfile.emplace(id,configured.sendDisabledThinking);
    service.buildParamsSegment=[baseSegment,
        sendDisabledByProfile=std::move(sendDisabledByProfile),
        selectedName=modelName,legacyImplicit=profile.legacyImplicitProfile,
        implicitDisabled=profile.sendDisabledThinking](const Json& retry,
        requests::RequestCapture& capture,
        const requests::BuilderDependencies& builder,
        const requests::WireProtocol& wire,
        const requests::BuilderHeaders& headers){
      auto params=baseSegment(retry,capture,builder,wire,headers);
      const auto& requested=value::property(retry,"model");
      const auto name=requested.is_string()?requested.get<std::string>():selectedName;
      const auto found=sendDisabledByProfile.find(name);
      const bool sendDisabledThinking=found!=sendDisabledByProfile.end()?
        found->second:legacyImplicit&&implicitDisabled;
      if(sendDisabledThinking&&
          (value::property(capture.thinkingConfig,"type")=="disabled"||
           capture.disableThinking))
        params["thinking"]={{"type","disabled"}};
      return params;
    };
  }
  const auto defaultClient=runtime.config.client;
  std::optional<std::string> runtimeToken;
  if(const auto token=runtime.runtimeAuthTokens.find(modelName);
      token!=runtime.runtimeAuthTokens.end())runtimeToken=token->second;
  service.createClient=[&local,profile,defaultClient,runtimeToken,modelName]{
    auto config=profile.client.value_or(defaultClient);
    const auto token=profile.authHeader=="none"?std::string{}
      :(runtimeToken?*runtimeToken:local.imports_.authTokenForModel
        ?local.imports_.authTokenForModel(modelName)
        :local.imports_.authToken());
    if(profile.authHeader=="authorization")config.authToken=token;
    else if(profile.authHeader=="x-api-key"){
      config.authToken.clear();
      if(!token.empty())config.headers["x-api-key"]=token;
      bool hasVersion=false;
      for(auto it=config.headers.begin();it!=config.headers.end();++it){
        auto name=it.key();
        std::transform(name.begin(),name.end(),name.begin(),
          [](unsigned char value){return static_cast<char>(std::tolower(value));});
        if(name=="anthropic-version")hasVersion=true;
      }
      if(!hasVersion)config.headers["anthropic-version"]="2023-06-01";
    }else config.authToken.clear();
    {
      std::lock_guard lock(local.mutex_);
      config.disableKeepAlive=local.state_.keepAliveDisabled;
    }
    if(local.imports_.configureClientNetwork)
      local.imports_.configureClientNetwork(config);
    return std::make_shared<transport::Client>(std::move(config));
  };
  service.normalizeFallbackContent=service.reducer.normalizeContent;
  service.getAssistantMessageFromError=[errors=errors,messages=messages](
      const api_errors::Failure& failure,const std::string& model)mutable{
    errors.messageServices=&messages;
    return api_errors::getAssistantMessageFromError(failure,model,errors);
  };
  service.extractQuotaStatusFromError=[&local](const auto& error){
    std::lock_guard lock(local.mutex_);local.state_.lastQuotaError=error;
  };
  service.markGadgetsSentToAPIState=[&local]{
    {std::lock_guard lock(local.mutex_);local.state_.gadgetsSentToApi=true;}
    local.imports_.diagnostic("tools","sent_to_api");
  };
  service.hasBatContext=[bat=value::property(options,"agentId")]{
    return value::truthy(bat);
  };
  service.setLastMainRequestId=[&local](const auto& id){
    std::lock_guard lock(local.mutex_);local.state_.lastMainRequestId=id;
    local.state_.lastCompletionMillis=platform::nowMillis();
  };
  service.nowMillis=platform::nowMillis;
  return service;
}
} // namespace batbox::application_model
