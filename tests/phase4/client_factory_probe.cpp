#include "client_factory/ClientFactory.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace batbox;
Json nullable(const std::optional<std::string>& value){
  return value?Json(*value):Json(nullptr);
}
Json planValue(const client_factory::ClientPlan& plan){
  Json credentials=nullptr;
  if(plan.awsCredentials)credentials=Json{{"accessKeyId",
      plan.awsCredentials->accessKeyId},{"secretAccessKey",
      plan.awsCredentials->secretAccessKey},{"sessionToken",
      plan.awsCredentials->flightToken}};
  return Json{{"provider",client_factory::providerName(plan.provider)},
    {"headers",plan.defaultHeaders},{"maxRetries",plan.maxRetries},
    {"timeout",plan.timeoutMs},{"proxy",plan.proxyFetchOptions},
    {"fetch",true},{"debugLogger",plan.stderrLogger},
    {"skipAuth",plan.skipAuth},{"awsRegion",nullable(plan.awsRegion)},
    {"awsCredentials",credentials},
    {"azureTokenProvider",plan.azureTokenProvider.has_value()},
    {"googleAuth",plan.googleAuthKind.empty()?Json(nullptr):
      Json(plan.googleAuthKind)},
    {"googleOptions",plan.googleAuthOptions.value_or(Json(nullptr))},
    {"region",nullable(plan.region)},{"apiKey",nullable(plan.apiKey)},
    {"authToken",nullable(plan.authToken)},
    {"baseUrl",nullable(plan.baseUrl)}};
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    if(test.value("op",std::string())=="logger"){
      Json entries=Json::array();
      client_factory::Services loggerServices;
      loggerServices.writeStderr=[&](const std::string& prefix,
          const std::string& message,const Json& args){
        Json row=Json::array({prefix,message});
        for(const auto& arg:args)row.push_back(arg);
        entries.push_back(row);
      };
      const auto logger=client_factory::createStderrLogger(loggerServices);
      const Json args=Json::array({1,Json{{"detail","value"}}});
      logger.error("message",args);
      logger.warn("message",args);
      logger.info("message",args);
      logger.debug("message",args);
      std::cout<<value::stringify(Json{{"value",entries},
        {"trace",Json::array()}})<<'\n';
      continue;
    }
    event_loop::Loop loop;
    Json trace=Json::array();
    Json value=nullptr;
    client_factory::Services services;
    services.environment=[&](const std::string& key)->Json{
      const auto& k=services.keys;
      const char* testName=nullptr;
      if(key==k.customHeaders)testName="customHeaders";
      else if(key==k.additionalProtection)testName="additionalProtection";
      else if(key==k.authToken)testName="authTokenEnv";
      else if(key==k.timeoutMs)testName="timeout";
      else if(key==k.useBedrock)testName="bedrock";
      else if(key==k.useFoundry)testName="foundry";
      else if(key==k.useVertex)testName="vertex";
      else if(key==k.smallFastAwsRegion)testName="smallRegion";
      else if(key==k.skipBedrockAuth)testName="skipBedrock";
      else if(key==k.bedrockBearerToken)testName="bedrockBearer";
      else if(key==k.foundryApiKey)testName="foundryApiKey";
      else if(key==k.skipFoundryAuth)testName="skipFoundry";
      else if(key==k.skipVertexAuth)testName="skipVertex";
      else if(key==k.projectUpper)testName="gcloudProject";
      else if(key==k.googleProjectUpper)testName="googleCloudProject";
      else if(key==k.projectLower)testName="gcloudProjectLower";
      else if(key==k.googleProjectLower)testName="googleCloudProjectLower";
      else if(key==k.keyFileUpper)testName="googleKeyFile";
      else if(key==k.keyFileLower)testName="googleKeyFileLower";
      else if(key==k.vertexProjectId)testName="vertexProject";
      else if(key==k.useStagingOAuth)testName="staging";
      else if(key==k.userType)return test.value("internal",false)
        ?Json("batbox"):Json("external");
      if(testName&&test.contains(testName))return test[testName];
      return nullptr;
    };
    services.wire.additionalProtection=
      "x-anthropic-additional-protection";
    services.getUserBat=[&]{trace.push_back("userAgent");
      return std::string("batbox-test/1");};
    services.logForDebugging=[&](const std::string&){
      trace.push_back("debug");};
    services.writeStderr=[](const std::string&,const std::string&,
      const Json&){};
    services.checkAndRefreshAuth=[&]{trace.push_back("refresh");
      event_loop::Deferred<event_loop::Unit> result(loop);
      if(test.value("failRefresh",false))result.reject(
        std::make_exception_ptr(std::runtime_error("refresh failed")));
      else result.resolve({});
      return result;};
    services.isSubscriber=[&]{trace.push_back("subscriber");
      return test.value("subscriber",false);};
    services.isNonInteractive=[&]{trace.push_back("nonInteractive");
      return test.value("nonInteractive",false);};
    services.apiKeyHelper=[&](bool nonInteractive){
      trace.push_back(Json::array({"helper",nonInteractive}));
      event_loop::Deferred<std::optional<std::string>> result(loop);
      if(test.contains("helperToken")&&!test["helperToken"].is_null())
        result.resolve(test["helperToken"].get<std::string>());
      else result.resolve(std::nullopt);
      return result;};
    services.getProxyFetchOptions=[&]{trace.push_back("proxy");
      return client_factory::NetworkOptions{
        test.value("proxy",Json::object()),{}};};
    services.smallFastModel=[&]{trace.push_back("smallModel");
      return std::string("fast-model");};
    services.awsRegion=[&]{trace.push_back("awsRegion");
      return std::string("us-east-1");};
    services.refreshAwsCredentials=[&]{trace.push_back("awsCredentials");
      event_loop::Deferred<std::optional<client_factory::Credentials>>
        result(loop);
      if(test.contains("awsCredentials")&&
          !test["awsCredentials"].is_null()){
        const auto& row=test["awsCredentials"];
        result.resolve(client_factory::Credentials{
          row.value("accessKeyId",std::string()),
          row.value("secretAccessKey",std::string()),
          row.value("sessionToken",std::string())});
      }else result.resolve(std::nullopt);
      return result;};
    services.refreshGcpCredentials=[&]{trace.push_back("gcpCredentials");
      event_loop::Deferred<event_loop::Unit> result(loop);
      result.resolve({});return result;};
    services.loadSdk=[&](client_factory::Provider provider){
      std::string module;
      switch(provider){
        case client_factory::Provider::Bedrock:
          module="@anthropic-ai/bedrock-sdk";break;
        case client_factory::Provider::Foundry:
          module="@anthropic-ai/foundry-sdk";break;
        case client_factory::Provider::Vertex:
          module="@anthropic-ai/vertex-sdk";break;
        case client_factory::Provider::Direct:break;
      }
      trace.push_back(Json::array({"load",module}));
      event_loop::Deferred<event_loop::Unit> result(loop);
      result.resolve({});return result;};
    services.loadGoogleAuthLibrary=[&]{
      trace.push_back(Json::array({"load","google-auth-library"}));
      event_loop::Deferred<event_loop::Unit> result(loop);
      result.resolve({});return result;};
    services.makeAzureTokenProvider=[&]{
      trace.push_back(Json::array({"load","@azure/identity"}));
      trace.push_back("azureProvider");
      event_loop::Deferred<client_factory::TokenProvider> result(loop);
      result.resolve([&]{event_loop::Deferred<std::string> token(loop);
        token.resolve("azure");return token;});
      return result;};
    services.makeGoogleAuth=[&](const Json&){trace.push_back("googleAuth");
      return client_factory::GoogleAuth{[&]{
        event_loop::Deferred<Json> headers(loop);
        headers.resolve(Json::object());return headers;}};};
    services.vertexRegionForModel=[&](const std::optional<std::string>& model){
      trace.push_back(Json::array({"vertexRegion",
        model?Json(*model):Json(nullptr)}));
      return std::string("us-east5");};
    services.getApiKey=[&]()->std::optional<std::string>{
      trace.push_back("apiKey");
      if(!test.contains("configApiKey")||test["configApiKey"].is_null())
        return std::nullopt;
      return test["configApiKey"].get<std::string>();};
    services.getOAuthTokens=[&]()->std::optional<Json>{
      trace.push_back("oauthTokens");
      if(!test.contains("oauthTokens")||test["oauthTokens"].is_null())
        return std::nullopt;
      return test["oauthTokens"];};
    services.oauthBaseApiUrl=[&]{trace.push_back("oauthConfig");
      return std::string("https://staging.example.test");};
    services.debugToStderr=[&]{trace.push_back("debugStderr");
      return test.value("debugStderr",false);};
    auto capture=[&](const client_factory::ClientPlan& plan){
      value=planValue(plan);
      trace.push_back(Json::array({"construct",
        client_factory::providerName(plan.provider)}));
      return std::make_shared<transport::Client>(transport::ClientConfig{});
    };
    services.makeProviderClient=capture;
    services.makeDirectNetworkClient=capture;
    client_factory::Inputs inputs;
    inputs.maxRetries=test.value("maxRetries",0);
    if(test.contains("apiKey")&&!test["apiKey"].is_null())
      inputs.apiKey=test["apiKey"].get<std::string>();
    if(test.contains("model")&&!test["model"].is_null())
      inputs.model=test["model"].get<std::string>();
    inputs.source="comparison";
    inputs.fetchOverride=std::make_shared<int>(1);
    const auto operation=client_factory::getBatboxClient(loop,inputs,services);
    bool done=false;
    Json outcome;
    operation.then([&](const std::shared_ptr<transport::Client>*,
        const std::exception_ptr& error){
      if(error){
        try{std::rethrow_exception(error);}catch(const std::exception& caught){
          outcome={{"error",caught.what()},{"trace",trace}};
        }
      }else outcome={{"value",value},{"trace",trace}};
      done=true;
    });
    for(int i=0;i<100&&!done;++i)loop.runReady();
    if(!done)throw std::runtime_error("Client factory did not settle");
    std::cout<<value::stringify(outcome)<<'\n';
  }
}
