#include "client_factory/ClientFactory.hpp"
#include "utils/envUtils.hpp"
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::client_factory {
namespace {
template<class T>const T& required(const T& binding,const char* name){
  if(!binding)throw std::invalid_argument(std::string("Missing ")+name+
    " client-factory binding");
  return binding;
}
class Creation:public std::enable_shared_from_this<Creation>{
public:
  Creation(event_loop::Loop& loop,Inputs input,Services services):
    loop_(loop),input_(std::move(input)),services_(std::move(services)),
    output_(loop){}
  event_loop::Deferred<std::shared_ptr<transport::Client>> start(){
    try{
      if(!loop_.isOwnerThread())throw std::logic_error(
        "Client factory must start on event loop owner");
      plan_.defaultHeaders=Json::object();
      plan_.defaultHeaders["User-Agent"]=required(
        services_.getUserBat,"user agent")();
      const auto custom=transport::getCustomHeaders(
        stringEnv(services_.keys.customHeaders));
      for(auto it=custom.begin();it!=custom.end();++it)
        plan_.defaultHeaders[it.key()]=it.value();
      if(services_.logForDebugging)
        services_.logForDebugging(
          "[API:request] Creating client, BATBOX_CUSTOM_HEADERS present: "+
          std::string(rawTruthyEnv(services_.keys.customHeaders)?"true":"false")+
          ", has Authorization header: "+
          std::string(value::truthy(value::property(custom,"Authorization"))
            ?"true":"false"));
      if(truthyEnv(services_.keys.additionalProtection)){
        if(services_.wire.additionalProtection.empty())
          throw std::invalid_argument(
            "Missing additional-protection wire header binding");
        plan_.defaultHeaders[services_.wire.additionalProtection]="true";
      }
      if(services_.logForDebugging)
        services_.logForDebugging("[API:auth] OAuth token check starting");
      required(services_.checkAndRefreshAuth,"OAuth refresh")().then(
        [self=shared_from_this()](const event_loop::Unit*,
            const std::exception_ptr& error){
          if(error){self->output_.reject(error);return;}
          self->afterRefresh();
        });
    }catch(...){output_.reject(std::current_exception());}
    return output_;
  }
private:
  event_loop::Loop& loop_;
  Inputs input_;
  Services services_;
  ClientPlan plan_;
  bool providerDebugEvaluated_=false;
  event_loop::Deferred<std::shared_ptr<transport::Client>> output_;
  Json env(const std::string& name)const{
    return required(services_.environment,"environment")(name);
  }
  std::string stringEnv(const std::string& name)const{
    const auto value=env(name);
    return value.is_string()?value.get<std::string>():std::string();
  }
  bool truthyEnv(const std::string& name)const{
    return isEnvTruthy(env(name));
  }
  bool rawTruthyEnv(const std::string& name)const{
    return value::truthy(env(name));
  }
  void afterRefresh(){
    try{
      if(services_.logForDebugging)
        services_.logForDebugging("[API:auth] OAuth token check complete");
      if(required(services_.isSubscriber,"subscriber")()){
        afterApiKeyHeaders();return;
      }
      const bool nonInteractive=required(services_.isNonInteractive,
        "non-interactive state")();
      const auto token=stringEnv(services_.keys.authToken);
      if(!token.empty()){
        plan_.defaultHeaders["Authorization"]="Bearer "+token;
        loop_.queueMicrotask([self=shared_from_this()]{
          self->afterApiKeyHeaders();});
        return;
      }
      required(services_.apiKeyHelper,"API-key helper")(nonInteractive).then(
        [self=shared_from_this()](const std::optional<std::string>* value,
            const std::exception_ptr& error){
          if(error){self->output_.reject(error);return;}
          if(value&&*value&&!(*value)->empty())
            self->plan_.defaultHeaders["Authorization"]="Bearer "+**value;
          self->afterApiKeyHeaders();
        });
    }catch(...){output_.reject(std::current_exception());}
  }
  void afterApiKeyHeaders(){
    try{
      plan_.fetchOverride=input_.fetchOverride;
      plan_.maxRetries=input_.maxRetries;
      const auto timeout=stringEnv(services_.keys.timeoutMs);
      plan_.timeoutMs=value::parseInt10(timeout.empty()?"600000":timeout);
      const auto network=required(services_.getProxyFetchOptions,
        "proxy fetch options")();
      plan_.proxyFetchOptions=network.values;
      plan_.proxyDispatcher=network.dispatcher;
      if(truthyEnv(services_.keys.useBedrock)){
        plan_.provider=Provider::Bedrock;
        loadSdk(Provider::Bedrock,[self=shared_from_this()]{
          self->afterBedrockLoad();});
      }else if(truthyEnv(services_.keys.useFoundry)){
        plan_.provider=Provider::Foundry;
        loadSdk(Provider::Foundry,[self=shared_from_this()]{
          self->afterFoundryLoad();});
      }else if(truthyEnv(services_.keys.useVertex)){
        plan_.provider=Provider::Vertex;
        beforeVertexLoad();
      }else{
        plan_.provider=Provider::Direct;
        finishDirect();
      }
    }catch(...){output_.reject(std::current_exception());}
  }
  void loadSdk(Provider provider,std::function<void()> next){
    required(services_.loadSdk,"SDK loader")(provider).then(
      [self=shared_from_this(),next=std::move(next)](
          const event_loop::Unit*,const std::exception_ptr& error){
        if(error){self->output_.reject(error);return;}
        try{next();}catch(...){self->output_.reject(std::current_exception());}
      });
  }
  void finishProvider(){
    if(!providerDebugEvaluated_){
      if(required(services_.debugToStderr,"debug stderr state")()){
        plan_.sdkLogger=createStderrLogger(services_);
        plan_.stderrLogger=true;
      }
      providerDebugEvaluated_=true;
    }
    auto client=required(services_.makeProviderClient,
      "provider SDK adapter")(plan_);
    if(!client)throw std::runtime_error("Provider SDK adapter returned no client");
    output_.resolve(std::move(client));
  }
  void afterBedrockLoad(){
    try{
      const auto small=required(services_.smallFastModel,"small fast model")();
      const auto override=stringEnv(services_.keys.smallFastAwsRegion);
      plan_.awsRegion=input_.model&&*input_.model==small&&!override.empty()
        ?override:required(services_.awsRegion,"AWS region")();
      plan_.skipAuth=truthyEnv(services_.keys.skipBedrockAuth);
      if(required(services_.debugToStderr,"debug stderr state")()){
        plan_.sdkLogger=createStderrLogger(services_);
        plan_.stderrLogger=true;
      }
      providerDebugEvaluated_=true;
      const auto bearer=stringEnv(services_.keys.bedrockBearerToken);
      if(!bearer.empty()){
        plan_.skipAuth=true;
        plan_.defaultHeaders["Authorization"]="Bearer "+bearer;
        finishProvider();return;
      }
      if(plan_.skipAuth){finishProvider();return;}
      required(services_.refreshAwsCredentials,"AWS credentials")().then(
        [self=shared_from_this()](const std::optional<Credentials>* value,
            const std::exception_ptr& error){
          if(error){self->output_.reject(error);return;}
          try{
            if(value&&*value)self->plan_.awsCredentials=**value;
            self->finishProvider();
          }catch(...){self->output_.reject(std::current_exception());}
        });
    }catch(...){output_.reject(std::current_exception());}
  }
  void afterFoundryLoad(){
    try{
      if(!rawTruthyEnv(services_.keys.foundryApiKey)){
        if(truthyEnv(services_.keys.skipFoundryAuth)){
          const auto dispatcher=loop_.dispatcher();
          plan_.azureTokenProvider=[dispatcher]{
            event_loop::Deferred<std::string> value(dispatcher);
            value.resolve("");return value;
          };
        }else{
          required(services_.makeAzureTokenProvider,
            "Azure token provider")().then(
              [self=shared_from_this()](const TokenProvider* value,
                  const std::exception_ptr& error){
                if(error){self->output_.reject(error);return;}
                try{
                  if(!value)throw std::runtime_error(
                    "Azure token provider import returned no value");
                  self->plan_.azureTokenProvider=*value;
                  self->finishProvider();
                }catch(...){self->output_.reject(std::current_exception());}
              });
          return;
        }
      }
      finishProvider();
    }catch(...){output_.reject(std::current_exception());}
  }
  void beforeVertexLoad(){
    try{
      if(truthyEnv(services_.keys.skipVertexAuth)){
        loadVertexSdks();return;
      }
      required(services_.refreshGcpCredentials,"GCP credentials")().then(
        [self=shared_from_this()](const event_loop::Unit*,
            const std::exception_ptr& error){
          if(error){self->output_.reject(error);return;}
          self->loadVertexSdks();
        });
    }catch(...){output_.reject(std::current_exception());}
  }
  void loadVertexSdks(){
    try{
      auto sdk=required(services_.loadSdk,"SDK loader")(Provider::Vertex);
      auto auth=required(services_.loadGoogleAuthLibrary,
        "Google auth library")();
      struct Both{int remaining=2;bool finished=false;};
      auto both=std::make_shared<Both>();
      auto done=[self=shared_from_this(),both](const std::exception_ptr& error){
        if(both->finished)return;
        if(error){both->finished=true;self->output_.reject(error);return;}
        if(--both->remaining==0){
          both->finished=true;self->afterVertexLoad();
        }
      };
      sdk.then([done](const event_loop::Unit*,const std::exception_ptr& error){
        done(error);});
      auth.then([done](const event_loop::Unit*,const std::exception_ptr& error){
        done(error);});
    }catch(...){output_.reject(std::current_exception());}
  }
  void afterVertexLoad(){
    try{
      const bool projectEnv=rawTruthyEnv(services_.keys.projectUpper)||
        rawTruthyEnv(services_.keys.googleProjectUpper)||
        rawTruthyEnv(services_.keys.projectLower)||
        rawTruthyEnv(services_.keys.googleProjectLower);
      const bool keyFile=rawTruthyEnv(services_.keys.keyFileUpper)||
        rawTruthyEnv(services_.keys.keyFileLower);
      Json options={{"scopes",Json::array({
        "https://www.googleapis.com/auth/cloud-platform"})}};
      if(truthyEnv(services_.keys.skipVertexAuth)){
        const auto dispatcher=loop_.dispatcher();
        plan_.googleAuth=GoogleAuth{[dispatcher]{
          event_loop::Deferred<Json> value(dispatcher);
          value.resolve(Json::object());return value;}};
        plan_.googleAuthKind="mock";
      }else{
        if(!projectEnv&&!keyFile){
          const auto fallback=stringEnv(services_.keys.vertexProjectId);
          if(!fallback.empty()){
            options["projectId"]=fallback;
            plan_.googleProjectIdFallback=fallback;
          }
        }
        plan_.googleAuth=required(services_.makeGoogleAuth,
          "Google auth constructor")(options);
        plan_.googleAuthKind="real";
        plan_.googleAuthOptions=options;
      }
      plan_.region=required(services_.vertexRegionForModel,
        "Vertex region")(input_.model);
      finishProvider();
    }catch(...){output_.reject(std::current_exception());}
  }
  void finishDirect(){
    try{
      if(required(services_.isSubscriber,"subscriber")()){
        plan_.apiKey=std::nullopt;
      }else if(input_.apiKey&& !input_.apiKey->empty()){
        plan_.apiKey=*input_.apiKey;
      }else{
        plan_.apiKey=required(services_.getApiKey,"API key")();
      }
      if(required(services_.isSubscriber,"subscriber")()){
        const auto tokens=required(services_.getOAuthTokens,
          "OAuth tokens")();
        if(tokens&&tokens->is_object()){
          const auto& access=value::property(*tokens,"accessToken");
          if(access.is_string())plan_.authToken=access.get<std::string>();
        }
      }
      if(env(services_.keys.userType)==services_.keys.internalUserType&&
          truthyEnv(services_.keys.useStagingOAuth))
        plan_.baseUrl=required(services_.oauthBaseApiUrl,
          "OAuth base URL")();
      if(required(services_.debugToStderr,"debug stderr state")()){
        plan_.sdkLogger=createStderrLogger(services_);
        plan_.stderrLogger=true;
      }
      output_.resolve(makeIndigenousDirectClient(plan_,services_));
    }catch(...){output_.reject(std::current_exception());}
  }
};
}
event_loop::Deferred<std::shared_ptr<transport::Client>> getBatboxClient(
    event_loop::Loop& loop,Inputs inputs,Services services){
  return std::make_shared<Creation>(loop,std::move(inputs),
    std::move(services))->start();
}
}
