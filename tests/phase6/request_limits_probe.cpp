#include "application_config/EndpointStore.hpp"
#include "application_model/LocalModelService.hpp"
#include "messages/Messages.hpp"
#include "model/Lifecycle.hpp"
#include "providers/ModelRequestSettings.hpp"
#include "query/SimpleQueriesBinding.hpp"
#include "runtime/Platform.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using std::chrono::milliseconds;
namespace {
void require(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
template<class Callback>void requireThrows(Callback callback,const std::string& message){
  try{callback();}catch(const std::invalid_argument&){return;}
  throw std::runtime_error(message);
}
void checkResolution(){
  for(const auto* url:{"http://127.0.0.1:8000","http://localhost:1234/v1",
      "http://192.168.1.20:8000","http://10.0.0.5","http://172.20.1.1:9000",
      "http://isnotreal:8000","http://gpu.local:8000","http://box.lan",
      "http://[::1]:8000","http://[fd12::1]:8000","http://100.101.102.103:8000",
      "http://gpu.tail1234.ts.net:8000"})
    require(providers::localNetworkUrl(url),std::string("Not local: ")+url);
  for(const auto* url:{"https://api.anthropic.com","https://openrouter.ai/api/v1",
      "http://172.32.0.1","http://8.8.8.8","https://example.com:8000"})
    require(!providers::localNetworkUrl(url),std::string("Local: ")+url);

  application::InferenceProfile remote;
  remote.baseUrl="https://api.example.com";
  auto settings=providers::requestSettings(remote);
  application::InferenceProfile local;
  local.baseUrl="http://192.168.1.20:8000";
  const auto localSettings=providers::requestSettings(local);
  require(!settings.local&&localSettings.local,"Local detection changed");
  // docs/request-limits.md: the same time limits for every endpoint.
  for(const auto& defaults:{settings,localSettings})
    require(!defaults.nonStreamingFallback&&
      defaults.requestTimeout==milliseconds(0)&&
      defaults.streamIdleTimeout==std::chrono::hours(6)&&
      defaults.connectTimeout==std::chrono::minutes(2)&&
      defaults.nonStreamingTimeout==milliseconds(0)&&!defaults.maxRetries,
      "Endpoint defaults differ from docs/request-limits.md");
  transport::ClientConfig unset;
  require(unset.requestTimeout==settings.requestTimeout&&
    unset.streamIdleTimeout==settings.streamIdleTimeout&&
    unset.connectTimeout==settings.connectTimeout,"Client defaults differ from endpoint defaults");
  require(model::watchdogSettings(Json{{"BATBOX_ENABLE_STREAM_WATCHDOG","1"}}).enabled==false,
    "The model-layer stream watchdog ran without a configured duration");
  application::InferenceProfile preset;
  preset.baseUrl="https://gpu.example.com";preset.preset="vllm";
  require(providers::requestSettings(preset).local,"A local catalog preset was not local");

  const Json row={{"local",false},{"nonstreaming_fallback",true},
    {"request_timeout_ms",0},{"stream_idle_timeout_ms",900000},
    {"connect_timeout_ms",3000},{"nonstreaming_timeout_ms",1800000},{"max_retries",1}};
  local.limits=application::parseRequestLimits(row,"test");
  settings=providers::requestSettings(local);
  require(!settings.local&&settings.nonStreamingFallback&&
    settings.requestTimeout==milliseconds(0)&&
    settings.streamIdleTimeout==milliseconds(900000)&&
    settings.connectTimeout==milliseconds(3000)&&
    settings.nonStreamingTimeout==milliseconds(1800000)&&settings.maxRetries==1u,
    "Configured request limits were not applied");
  require(application::requestLimitsDocument(local.limits)==row,
    "Request limits did not round-trip");
  require(application::requestLimitsDocument({}).empty(),"Unset limits were written");
  requireThrows([]{application::parseRequestLimits(Json{{"local","yes"}},"t");},
    "A non-boolean local was accepted");
  requireThrows([]{application::parseRequestLimits(Json{{"max_retries",-1}},"t");},
    "A negative retry count was accepted");
  requireThrows([]{application::parseRequestLimits(Json{{"stream_idle_timeout_ms",1.5}},"t");},
    "A fractional duration was accepted");
  requireThrows([]{application::parseRequestLimits(Json{{"connect_timeout_ms",0}},"t");},
    "A zero connect timeout was accepted");

  application::Config config;
  application_config::loadEndpoints(config,Json{{"endpoints",{{"gpu",{
    {"label","GPU box"},{"base_url","http://192.168.1.20:8000"},
    {"provider","openai-chat"},{"context_window",100000},{"max_output_tokens",8000},
    {"models",Json::array({"qwen"})},{"stream_idle_timeout_ms",600000},
    {"nonstreaming_fallback",true}}}}}});
  const auto& endpoint=config.endpoints.at("gpu");
  require(endpoint.limits.streamIdleTimeoutMs==600000u&&
    endpoint.limits.nonStreamingFallback==true,"Endpoint limits were not read");
  const auto document=application_config::endpointDocument(config.endpoints);
  require(document.at("gpu").at("stream_idle_timeout_ms")==600000&&
    document.at("gpu").at("nonstreaming_fallback")==true&&
    !document.at("gpu").contains("local"),"Endpoint limits were not saved back");
  application_model::ModelProfile profile;
  providers::applyRequestSettings(profile,application_config::inferenceProfile(endpoint,"qwen"));
  require(profile.local&&profile.nonStreamingFallback&&profile.nonStreamingTimeoutMs==0&&
    profile.client->streamIdleTimeout==milliseconds(600000)&&
    profile.client->requestTimeout==milliseconds(0),"Endpoint limits did not reach the profile");
}
application_model::ModelProfile limitedProfile(const std::string& id,const std::string& base,
    const std::string& path,const Json& limits){
  application::InferenceProfile configured;
  configured.model=id;configured.baseUrl=base;configured.messagesPath=path;
  configured.limits=application::parseRequestLimits(limits,id);
  application_model::ModelProfile profile;
  profile.id=id;profile.wireModel=id;profile.contextTokens=200000;profile.maxOutputTokens=1000;
  profile.costs={{"inputTokens",0},{"outputTokens",0},{"promptCacheReadTokens",0},
    {"promptCacheWriteTokens",0},{"webSearchRequests",0}};
  profile.client.emplace();
  profile.client->baseUrl=base;profile.client->messagesPath=path;
  providers::applyRequestSettings(profile,configured);
  return profile;
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("Base URL required");
  checkResolution();
  const std::string base=argv[1];
  event_loop::Loop loop;
  application_model::Config config;
  config.client.baseUrl=base;config.client.messagesPath="/unused";
  const std::vector<std::pair<std::string,application_model::ModelProfile>> profiles{
    {"brokenLocal",limitedProfile("broken-local",base,"/broken-local",Json::object())},
    {"brokenRemote",limitedProfile("broken-remote",base,"/broken-remote",
      {{"local",false},{"nonstreaming_fallback",true}})},
    {"slowIdle",limitedProfile("slow-idle",base,"/slow-idle",
      {{"stream_idle_timeout_ms",400},{"max_retries",0}})},
    {"slowHeaders",limitedProfile("slow-headers",base,"/slow-headers",
      {{"stream_idle_timeout_ms",400}})},
    {"slowUnlimited",limitedProfile("slow-unlimited",base,"/slow-unlimited",
      {{"stream_idle_timeout_ms",0},{"request_timeout_ms",0}})}};
  for(const auto& [name,profile]:profiles)config.profiles.emplace(profile.id,profile);
  config.mainModel=config.smallModel="broken-local";
  config.retryWire.retryAfter="retry-after";
  config.retryWire.shouldRetry="x-should-retry";
  application_model::Imports imports;
  imports.authToken=[]{return std::string("test-token");};
  imports.invalidateAuthToken=[]{};
  imports.debug=[](const auto&,const auto&){};
  imports.diagnostic=[](const auto&,const auto&){};
  imports.normalizeAttachmentForAPI=[](const Json&)->Json{throw std::logic_error("attachment");};
  imports.normalizeGadgetInputForAPI=[](const gadgets::Gadget&,const Json&)->Json{
    throw std::logic_error("tool input");};
  imports.gadgetSchemaAsync=[](const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&)
      ->event_loop::Deferred<Json>{throw std::logic_error("tool schema");};
  imports.gadgetSchemaSync=[](const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&)
      ->Json{throw std::logic_error("tool schema");};
  auto service=std::make_shared<application_model::LocalModelService>(
    loop,std::move(config),std::move(imports));
  const auto services=service->makeQueryServices({},Json{{"model","broken-local"}});
  require(services.disableFallbackFeature&&
    services.environment.at("API_TIMEOUT_MS")=="0","Local fallback settings did not reach the model");
  const auto remote=service->makeQueryServices({},Json{{"model","broken-remote"}});
  require(!remote.disableFallbackFeature&&remote.environment.at("API_TIMEOUT_MS")=="0",
    "Remote fallback settings did not reach the model");
  const auto idle=service->makeQueryServices({},Json{{"model","slow-idle"}});
  require(idle.retry.env.at("BATBOX_MAX_RETRIES")=="0","max_retries did not reach the retry policy");
  messages::Services factories;
  factories.randomUUID=platform::randomUUID;
  factories.nowISO=[]{return platform::isoTimestamp();};
  Json output=Json::object();
  for(const auto& [name,profile]:profiles){
    query::ModelRequest request;
    request.messages=Json::array({messages::createUserMessage(Json{{"content","hi"}},factories)});
    request.systemPoetry=Json::array();
    request.thinkingConfig={{"type","disabled"}};
    request.options={{"model",profile.id},{"querySource","repl_main_thread"},
      {"enablePromptCaching",false}};
    std::exception_ptr failure;model::MessageRef answer;bool done=false;
    auto keep=loop.hold();
    query::collectLastAssistantMessage(loop,service->makeCursor(request)).then(
      [&](const model::MessageRef* value,const std::exception_ptr& error){
        failure=error;if(value)answer=*value;done=true;loop.stop();
      });
    loop.run();keep.release();
    require(done,"Model request did not settle: "+name);
    std::string text="error";
    if(!failure&&answer&&!value::truthy(value::property(*answer,"isApiErrorMessage"))){
      const auto& content=value::property(value::property(*answer,"message"),"content");
      if(content.is_array()&&!content.empty()&&content.at(0).contains("text"))
        text=content.at(0).at("text").get<std::string>();
    }
    output[name]=text;
  }
  std::cout<<output.dump()<<'\n';
}
