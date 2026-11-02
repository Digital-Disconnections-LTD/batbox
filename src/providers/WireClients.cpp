#include "providers/WireClients.hpp"
#include "application_config/EndpointStore.hpp"
#include "providers/cloud/CloudClients.hpp"
#include "transport/openai_chat/OpenAIChatClient.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace batbox::providers {
std::shared_ptr<transport::WireClient> makeWireClient(const WireTarget& target,
    transport::ClientConfig config){
  if(target.kind==transport::kProviderAnthropic)
    return std::make_shared<transport::AnthropicMessagesClient>(std::move(config));
  if(target.kind==transport::kProviderOpenAIChat)
    return std::make_shared<transport::openai_chat::OpenAIChatClient>(std::move(config),
      transport::chatQuirks(target.quirks));
  if(cloud::isCloudKind(target.kind)){
    auto token=config.authToken;
    if(token.empty()&&config.headers.contains("x-api-key")&&config.headers["x-api-key"].is_string())
      token=config.headers["x-api-key"].get<std::string>();
    return cloud::makeCloudClient({target.kind,target.region,target.project,target.models,token},
      std::move(config));
  }
  transport::validateProviderKind(target.kind);
  throw std::invalid_argument("Provider "+target.kind+" is not available in this build of Batbox");
}

void applyAuth(transport::ClientConfig& config,const std::string& kind,
    const std::string& authHeader,const std::string& token){
  if(authHeader=="authorization"){config.authToken=token;return;}
  config.authToken.clear();
  if(authHeader!="x-api-key")return;
  if(!token.empty())config.headers["x-api-key"]=token;
  bool hasVersion=false;
  for(auto it=config.headers.begin();it!=config.headers.end();++it){
    auto name=it.key();
    std::transform(name.begin(),name.end(),name.begin(),
      [](unsigned char value){return static_cast<char>(std::tolower(value));});
    if(name=="anthropic-version")hasVersion=true;
  }
  if(!hasVersion&&kind==transport::kProviderAnthropic)
    config.headers["anthropic-version"]="2023-06-01";
}

std::string endpointKey(const application::EndpointRecord& endpoint){
  if(endpoint.authHeader=="none")return {};
  if(!endpoint.authToken.empty()||endpoint.apiKeyEnv.empty())return endpoint.authToken;
#ifdef _WIN32
  return runtime::environmentUtf8(endpoint.apiKeyEnv.c_str());
#else
  const char* value=std::getenv(endpoint.apiKeyEnv.c_str());
  return value?std::string(value):std::string{};
#endif
}

transport::ClientConfig endpointConfig(const application::EndpointRecord& endpoint,
    std::chrono::milliseconds timeout){
  const auto profile=application_config::inferenceProfile(endpoint,
    endpoint.models.empty()?std::string{}:endpoint.models.front());
  transport::ClientConfig config;
  config.baseUrl=profile.baseUrl;
  config.messagesPath=profile.messagesPath;
  config.modelsPath=profile.modelsPath;
  config.headers=profile.headers.is_object()?profile.headers:Json::object();
  config.requestTimeout=timeout;
  config.connectTimeout=std::min(timeout,std::chrono::milliseconds(5000));
  applyAuth(config,endpoint.provider,endpoint.authHeader,endpointKey(endpoint));
  return config;
}

Json listEndpointModels(const application::EndpointRecord& endpoint,
    std::chrono::milliseconds timeout){
  return makeWireClient({endpoint.provider,endpoint.quirks,endpoint.region,endpoint.project,
    endpoint.models},endpointConfig(endpoint,timeout))->listModels();
}

std::string listingTarget(const application::EndpointRecord& endpoint){
  if(endpoint.provider==transport::kProviderBedrock)
    return "Amazon Bedrock"+(endpoint.region.empty()?std::string{}:" in "+endpoint.region)+
      " (signed with your AWS credentials)";
  if(endpoint.provider==transport::kProviderVertex)
    return "Vertex AI"+(endpoint.region.empty()?std::string{}:" in "+endpoint.region)+
      " (Claude models; no listing request)";
  try{return "GET "+application_config::modelsUrl(endpoint);}
  catch(const std::exception&){return "GET "+endpoint.baseUrl;}
}
} // namespace batbox::providers
