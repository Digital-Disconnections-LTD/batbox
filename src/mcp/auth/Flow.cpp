#include "mcp/auth/Flow.hpp"
#include "mcp/auth/Discovery.hpp"
#include "application/Async.hpp"
#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Url.hpp"
#include <condition_variable>
#include <cstdlib>

namespace batbox::mcp::auth {
namespace {
std::string value(const Json& object,const char* key){const auto& item=value::property(object,key);
  return item.is_string()?item.get<std::string>():std::string{};}
Json parameters(const std::string& url){
  const auto parsed=runtime::parseUrl(url);if(!parsed)throw std::runtime_error("Invalid OAuth callback URL");
  const auto search=value(*parsed,"search");Json result=Json::object();
  for(const auto& [key,text] : ::batbox::auth::oauth_client::parseFormQuery(
      search.starts_with('?')?search.substr(1):search))if(!result.contains(key))result[key]=text;
  result["path"]=parsed->at("pathname");return result;
}
struct Callback {
  std::mutex mutex;std::condition_variable ready;
  bool done=false;std::string code;std::exception_ptr error;
  void settle(std::string value,std::exception_ptr failure={}){
    std::lock_guard lock(mutex);if(done)return;
    done=true;code=std::move(value);error=std::move(failure);ready.notify_all();
  }
};
void accept(const std::shared_ptr<Callback>& pending,const Json& values,
    const std::string& state,bool manual){
  const auto error=value(values,"error");
  if(!error.empty())throw std::runtime_error("OAuth error: "+error+" - "+value(values,"error_description"));
  const auto code=value(values,"code");if(code.empty())return;
  if(value(values,"state")!=state)throw std::runtime_error("OAuth state mismatch - possible CSRF attack");
  (void)manual;pending->settle(code);
}
}
int findAvailablePort(const PortServices& services){
  if(!services.available||!services.random)throw std::invalid_argument("OAuth port discovery bindings required");
  if(services.configured){
    const auto port=value::parseInt10(*services.configured);
    if(port>0)return static_cast<int>(port);
  }
  const int minimum=services.windows?39152:49152,maximum=services.windows?49151:65535;
  const int range=maximum-minimum+1;
  for(int attempt=0;attempt<std::min(range,100);++attempt){
    const auto port=minimum+static_cast<int>(services.random()*range);
    if(services.available(port))return port;
  }
  if(services.available(3118))return 3118;
  throw std::runtime_error("No available ports for OAuth redirect");
}
event_loop::Deferred<event_loop::Unit> performMCPOAuthFlow(FlowInputs inputs){
  if(!inputs.owner.isOwnerThread()||!inputs.createServer||!inputs.findPort)
    throw std::invalid_argument("MCP OAuth flow owner and callback server required");
  auto host=std::make_shared<FlowInputs>(std::move(inputs));const auto owner=host->owner.dispatcher();
  return application::runInWorker<event_loop::Unit>(host->owner,[host,owner]{
    if(value::truthy(value::property(value::property(host->config,"oauth"),"xaa")))
      throw std::runtime_error("XAA MCP authentication provider is not connected in this native build");
    if(host->signal.stop_requested())throw AuthenticationCancelledError();
    Json stashed;
    {
      std::lock_guard lock(*host->services.storageMutex);
      const auto data=host->services.storage.read().value_or(Json::object());
      stashed=value::property(value::property(data,"mcpOAuth"),getServerKey(host->name,host->config));
      clearServerTokensFromLocalStorage(host->name,host->config,host->services.storage);
    }
    std::optional<std::string> resource;
    const auto stashedResource=value(value::property(stashed,"discoveryState"),"resourceMetadataUrl");
    if(!stashedResource.empty()&&runtime::parseUrl(stashedResource))resource=stashedResource;
    const auto configured=value::property(value::property(host->config,"oauth"),"callbackPort");
    const int port=configured.is_number_integer()?configured.get<int>():host->findPort();
    Provider provider(host->name,host->config,host->services,buildRedirectUri(port),true,host->skipBrowserOpen);
    try{provider.setMetadata(fetchAuthServerMetadata(host->config.at("url").get<std::string>(),
      host->config,resource,host->services.fetch,host->signal));}catch(const std::exception&){}
    const auto state=provider.state();auto callback=std::make_shared<Callback>();
    auto server=application::onOwnerAndWait<std::shared_ptr<::batbox::auth::oauth_listener::Server>>(
      owner,host->createServer);
    const auto cleanup=[owner,server]{
      application::onOwnerAndWait<void>(owner,[server]{server->removeAllListeners();server->close();});
    };
    try{
      application::onOwnerAndWait<void>(owner,[host,server,callback,state]{
        server->onError([callback](const std::string& message){
          callback->settle({},std::make_exception_ptr(std::runtime_error("OAuth callback server failed: "+message)));});
        server->onRequest([callback,state](const ::batbox::auth::oauth_listener::Request& request){
          try{
            auto values=parameters("http://localhost"+request.url);
            if(values.at("path")!="/callback"){
              request.response->writeHead(404,{{"Content-Type","text/plain"}});request.response->end("Not found");return;
            }
            accept(callback,values,state,false);
            request.response->writeHead(200,{{"Content-Type","text/html"}});
            request.response->end("<h1>Authentication Successful</h1><p>You can close this window. Return to Batbox.</p>");
          }catch(const std::exception& failure){
            request.response->writeHead(400,{{"Content-Type","text/html"}});
            request.response->end("<h1>Authentication Error</h1><p>Please return to Batbox and try again.</p>");
            callback->settle({},std::make_exception_ptr(std::runtime_error(failure.what())));
          }
        });
        if(host->onWaitingForCallback)host->onWaitingForCallback([callback,state](const std::string& url){
          Json values;
          try{values=parameters(url);}catch(...){return;}
          try{accept(callback,values,state,true);}catch(...){callback->settle({},std::current_exception());}
        });
      });
      (void)application::awaitOnOwner<int>(owner,[server,port]{return server->start(port);});
      OAuthFlight flight(provider,host->services.fetch,host->signal);
      const auto scope=value(stashed,"stepUpScope");
      flight.begin(scope.empty()?std::nullopt:std::optional(scope),resource);
      std::unique_lock lock(callback->mutex);
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
      while(!callback->done){
        if(host->signal.stop_requested())throw AuthenticationCancelledError();
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Authentication timeout");
        callback->ready.wait_for(lock,std::chrono::milliseconds(50));
      }
      if(callback->error)std::rethrow_exception(callback->error);
      const auto code=callback->code;lock.unlock();cleanup();flight.finish(code);
      return event_loop::Unit{};
    }catch(...){try{cleanup();}catch(...){}throw;}
  });
}
} // namespace batbox::mcp::auth
