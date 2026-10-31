#pragma once
#include "event_loop/Deferred.hpp"
#include "transport/Client.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::client_factory {
enum class Provider {Direct,Bedrock,Foundry,Vertex};
struct Credentials {
  std::string accessKeyId,secretAccessKey,flightToken;
};
using TokenProvider=std::function<event_loop::Deferred<std::string>()>;
using LogMethod=std::function<void(const std::string&,const Json&)>;
struct SdkLogger {
  LogMethod error,warn,info,debug;
};
struct GoogleAuth {
  std::function<event_loop::Deferred<Json>()> getRequestHeaders;
};
struct NetworkOptions {
  Json values=Json::object();
  std::shared_ptr<void> dispatcher;
};
struct Inputs {
  std::optional<std::string> apiKey,model,source;
  int maxRetries=0;
  // The Indigenous HTTP transport requires an explicit network adapter
  // whenever an override is present.
  std::shared_ptr<void> fetchOverride;
};
struct EnvironmentKeys {
  std::string customHeaders="BATBOX_CUSTOM_HEADERS";
  std::string additionalProtection="BATBOX_ADDITIONAL_PROTECTION";
  std::string authToken="BATBOX_AUTH_TOKEN";
  std::string timeoutMs="API_TIMEOUT_MS";
  std::string useBedrock="BATBOX_USE_BEDROCK";
  std::string useFoundry="BATBOX_USE_FOUNDRY";
  std::string useVertex="BATBOX_USE_VERTEX";
  std::string smallFastAwsRegion="BATBOX_SMALL_FAST_MODEL_AWS_REGION";
  std::string skipBedrockAuth="BATBOX_SKIP_BEDROCK_AUTH";
  std::string bedrockBearerToken="AWS_BEARER_TOKEN_BEDROCK";
  std::string foundryApiKey="BATBOX_FOUNDRY_API_KEY";
  std::string skipFoundryAuth="BATBOX_SKIP_FOUNDRY_AUTH";
  std::string skipVertexAuth="BATBOX_SKIP_VERTEX_AUTH";
  std::string projectUpper="GCLOUD_PROJECT";
  std::string googleProjectUpper="GOOGLE_CLOUD_PROJECT";
  std::string projectLower="gcloud_project";
  std::string googleProjectLower="google_cloud_project";
  std::string keyFileUpper="GOOGLE_APPLICATION_CREDENTIALS";
  std::string keyFileLower="google_application_credentials";
  std::string vertexProjectId="BATBOX_VERTEX_PROJECT_ID";
  std::string userType="USER_TYPE";
  std::string internalUserType="batbox";
  std::string useStagingOAuth="USE_STAGING_OAUTH";
};
struct WireHeaders {
  std::string additionalProtection; // provider protocol name, supplied by backend
  std::string apiKey="x-api-key";
};
struct ClientPlan {
  Provider provider=Provider::Direct;
  Json defaultHeaders=Json::object();
  Json proxyFetchOptions=Json::object();
  std::shared_ptr<void> proxyDispatcher;
  int maxRetries=0;
  double timeoutMs=600000;
  bool dangerouslyAllowBrowser=true;
  bool stderrLogger=false;
  std::optional<SdkLogger> sdkLogger;
  bool skipAuth=false;
  std::optional<std::string> apiKey,authToken,baseUrl,awsRegion,region;
  std::optional<Credentials> awsCredentials;
  std::optional<TokenProvider> azureTokenProvider;
  std::optional<GoogleAuth> googleAuth;
  std::optional<Json> googleAuthOptions;
  std::string googleAuthKind;
  std::optional<std::string> googleProjectIdFallback;
  std::shared_ptr<void> fetchOverride;
};
struct Services {
  std::function<Json(const std::string&)> environment;
  std::function<std::string()> getUserBat;
  std::function<void(const std::string&)> logForDebugging;
  std::function<void(const std::string&,const std::string&,const Json&)>
    writeStderr;
  std::function<event_loop::Deferred<event_loop::Unit>()> checkAndRefreshAuth;
  std::function<bool()> isSubscriber,isNonInteractive,debugToStderr;
  std::function<event_loop::Deferred<std::optional<std::string>>(bool)>
    apiKeyHelper;
  std::function<NetworkOptions()> getProxyFetchOptions;
  std::function<std::string()> smallFastModel,awsRegion,oauthBaseApiUrl;
  std::function<std::string(const std::optional<std::string>&)>
    vertexRegionForModel;
  std::function<std::optional<std::string>()> getApiKey;
  std::function<std::optional<Json>()> getOAuthTokens;
  std::function<event_loop::Deferred<event_loop::Unit>(Provider)> loadSdk;
  std::function<event_loop::Deferred<event_loop::Unit>()> loadGoogleAuthLibrary;
  std::function<event_loop::Deferred<std::optional<Credentials>>()>
    refreshAwsCredentials;
  std::function<event_loop::Deferred<event_loop::Unit>()>
    refreshGcpCredentials;
  std::function<event_loop::Deferred<TokenProvider>()> makeAzureTokenProvider;
  std::function<GoogleAuth(const Json&)> makeGoogleAuth;
  std::function<std::shared_ptr<transport::Client>(const ClientPlan&)>
    makeProviderClient,makeDirectNetworkClient;
  EnvironmentKeys keys;
  WireHeaders wire;
  std::string directBaseUrl="http://127.0.0.1:8000";
  std::string messagesPath="/v1/messages";
  transport::MessagesProtocol messagesProtocol;
};

std::string providerName(Provider provider);
SdkLogger createStderrLogger(const Services& services);
std::shared_ptr<transport::Client> makeIndigenousDirectClient(
  const ClientPlan& plan,const Services& services);
event_loop::Deferred<std::shared_ptr<transport::Client>> getBatboxClient(
  event_loop::Loop& loop,Inputs inputs,Services services);
}
