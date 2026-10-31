#include "auth/oauth_listener/AuthCodeListener.hpp"
#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Url.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::auth::oauth_listener {
namespace {
std::optional<std::string> queryValue(
    const auth::oauth_client::QueryEntries& entries,const char* key){
  for(const auto& [name,value]:entries)if(name==key)return value;
  return std::nullopt;
}
bool contains(const std::vector<std::string>& scopes,
    const std::string& target){
  return std::find(scopes.begin(),scopes.end(),target)!=scopes.end();
}
}
AuthCodeListener::AuthCodeListener(event_loop::Loop& loop,Services services,
    std::string callbackPath):loop_(loop),services_(std::move(services)),
    callbackPath_(std::move(callbackPath)){
  if(!services_.createServer||!services_.getConfig||!services_.logError||
      services_.inferenceScope.empty())
    throw std::invalid_argument("Missing OAuth listener binding");
  server_=services_.createServer();
  if(!server_)throw std::invalid_argument("Missing OAuth callback server");
}
AuthCodeListener::~AuthCodeListener(){
  try{close();}catch(...){}
  lifetime_.reset();
}
event_loop::Deferred<int> AuthCodeListener::start(std::optional<int> port){
  event_loop::Deferred<int> output(loop_);
  try{
    server_->start(port).then([this,alive=std::weak_ptr<int>(lifetime_),
        output](const int* assigned,
        const std::exception_ptr& error){
      if(alive.expired())return;
      if(error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& cause){output.reject(
          std::make_exception_ptr(std::runtime_error(
            "Failed to start OAuth callback server: "+
            std::string(cause.what()))));}
        return;
      }
      port_=*assigned;output.resolve(port_);
    });
  }catch(const std::exception& cause){
    output.reject(std::make_exception_ptr(std::runtime_error(
      "Failed to start OAuth callback server: "+
      std::string(cause.what()))));
  }
  return output;
}
int AuthCodeListener::getPort()const{return port_;}
bool AuthCodeListener::hasPendingResponse()const{
  return static_cast<bool>(pendingResponse_);
}
event_loop::Deferred<std::string> AuthCodeListener::waitForAuthorization(
    const std::string& state,
    const std::function<event_loop::Deferred<event_loop::Unit>()>& onReady){
  event_loop::Deferred<std::string> output(loop_);
  pendingPromise_=output;
  expectedState_=state;
  try{
    server_->onRequest([this,alive=std::weak_ptr<int>(lifetime_)](
        const Request& request){
      if(!alive.expired())handleRedirect(request);});
    server_->onError([this,alive=std::weak_ptr<int>(lifetime_)](
        const std::string& error){
      if(!alive.expired())handleError(error);});
    if(!onReady)throw std::invalid_argument("Missing OAuth ready callback");
    (void)onReady();
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void AuthCodeListener::handleSuccessRedirect(
    const std::vector<std::string>& scopes,
    const std::function<void(Response&,const std::vector<std::string>&)>&
      customHandler){
  if(!pendingResponse_)return;
  if(customHandler){
    customHandler(*pendingResponse_,scopes);
    pendingResponse_.reset();return;
  }
  const auto config=services_.getConfig();
  const auto& url=contains(scopes,services_.inferenceScope)?
    config.appSuccessUrl:config.consoleSuccessUrl;
  pendingResponse_->writeHead(302,Json{{"Location",url}});
  pendingResponse_->end();
  pendingResponse_.reset();
}
void AuthCodeListener::handleErrorRedirect(){
  if(!pendingResponse_)return;
  const auto url=services_.getConfig().appSuccessUrl;
  pendingResponse_->writeHead(302,Json{{"Location",url}});
  pendingResponse_->end();
  pendingResponse_.reset();
}
void AuthCodeListener::handleRedirect(const Request& request){
  const auto base="http://"+(request.host.empty()?std::string("localhost"):
    request.host);
  const auto parsed=runtime::parseUrl(request.url,base);
  if(!parsed)throw std::invalid_argument("Invalid OAuth callback URL");
  if((*parsed)["pathname"]!=callbackPath_){
    request.response->writeHead(404,Json::object());
    request.response->end();return;
  }
  const auto params=auth::oauth_client::parseFormQuery(
    (*parsed)["search"].get<std::string>());
  validateAndRespond(queryValue(params,"code"),queryValue(params,"state"),
    request.response);
}
void AuthCodeListener::validateAndRespond(
    const std::optional<std::string>& code,
    const std::optional<std::string>& state,
    std::shared_ptr<Response> response){
  if(!code||code->empty()){
    response->writeHead(400,Json::object());
    response->end("Authorization code not found");
    reject("No authorization code received");return;
  }
  if(state!=expectedState_){
    response->writeHead(400,Json::object());
    response->end("Invalid state parameter");
    reject("Invalid state parameter");return;
  }
  pendingResponse_=std::move(response);
  resolve(*code);
}
void AuthCodeListener::handleError(const std::string& message){
  services_.logError(message);
  close();reject(message);
}
void AuthCodeListener::resolve(const std::string& code){
  if(!pendingPromise_)return;
  pendingPromise_->resolve(code);pendingPromise_.reset();
}
void AuthCodeListener::reject(const std::string& message){
  if(!pendingPromise_)return;
  pendingPromise_->reject(std::make_exception_ptr(
    std::runtime_error(message)));
  pendingPromise_.reset();
}
void AuthCodeListener::close(){
  if(pendingResponse_)handleErrorRedirect();
  if(server_){server_->removeAllListeners();server_->close();}
}
}
