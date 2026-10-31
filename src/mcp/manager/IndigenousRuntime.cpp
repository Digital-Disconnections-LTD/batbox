#include "mcp/manager/IndigenousRuntime.hpp"
#include "application/Async.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>

namespace batbox::mcp::manager {
IndigenousRuntime::IndigenousRuntime(IndigenousInputs inputs):inputs_(std::move(inputs)){
  if(!inputs_.owner.isOwnerThread()||inputs_.configHome.empty()||inputs_.projectRoot.empty())
    throw std::invalid_argument("MCP native owner, config home and project root required");
  if(!inputs_.diagnostic)inputs_.diagnostic=[](const std::string&,const std::string&){};
  if(!inputs_.changed)inputs_.changed=[](const std::vector<ServerResult>&){};
}
std::shared_ptr<IndigenousRuntime> IndigenousRuntime::create(IndigenousInputs inputs){
  auto self=std::shared_ptr<IndigenousRuntime>(new IndigenousRuntime(std::move(inputs)));
  self->initialize();return self;
}
void IndigenousRuntime::initialize(){
  loadConfiguration();const auto signal=lifetime_.get_token();const auto fetch=inputs_.fetch;
  http::Fetch cancellable=[signal,fetch](const http::Request& request){
    std::stop_source stopped;
    std::stop_callback lifetime(signal,[&]{stopped.request_stop();});
    std::stop_callback caller(request.signal,[&]{stopped.request_stop();});
    auto copy=request;copy.signal=stopped.get_token();return fetch(copy);
  };
  const auto diagnostic=inputs_.diagnostic;
  auth_=auth::makeIndigenousServices(inputs_.owner,inputs_.configHome,
    [diagnostic](const std::string& message){diagnostic("OAuth",message);},cancellable);
  const auto weak=weak_from_this();Inputs manager(inputs_.owner);
  manager.configurations=configurations_;
  manager.connect=[weak](const std::string& name,const Json& config,ClientHandlers handlers){
    auto self=weak.lock();if(!self)throw std::runtime_error("MCP runtime closed");
    return self->connect(name,config,std::move(handlers));};
  manager.refresh=[weak](const ServerResult& server,const std::string& kind){
    auto self=weak.lock();if(!self)throw std::runtime_error("MCP runtime closed");return self->refresh(server,kind);};
  manager.isDisabled=[weak](const std::string& name){const auto self=weak.lock();
    return !self||std::find(self->disabled_.begin(),self->disabled_.end(),Json(name))!=self->disabled_.end();};
  manager.setEnabled=[weak](const std::string& name,bool enabled){
    auto self=weak.lock();if(!self)return;
    auto& disabled=self->disabled_;
    disabled.erase(std::remove(disabled.begin(),disabled.end(),Json(name)),disabled.end());
    if(!enabled)disabled.push_back(name);
    self->writeDisabled();};
  manager.cancel=[weak]{if(auto self=weak.lock())self->lifetime_.request_stop();};
  manager.changed=inputs_.changed;manager.diagnostic=diagnostic;
  manager.notification=inputs_.notification;
  manager.serverRequest=[weak](const std::string& name,const Json& request){
    const auto self=weak.lock();if(!self)throw std::runtime_error("MCP runtime closed");
    if(self->inputs_.serverRequest)return self->inputs_.serverRequest(name,request);
    const auto& method=value::property(request,"method");
    if(method=="roots/list")return Json{{"roots",Json::array({{{"uri","file://"+
      runtime::pathToUtf8(self->inputs_.projectRoot)},{"name",runtime::pathToUtf8(self->inputs_.projectRoot.filename())}}})}};
    if(method=="ping")return Json::object();
    if(method=="elicitation/create")return Json{{"action","cancel"}};
    throw std::runtime_error("MCP client method unavailable");
  };
  manager.serverRequestAsync=[weak](const std::string& name,
      const Json& request,std::stop_token signal){
    auto self=weak.lock();if(!self)throw std::runtime_error("MCP runtime closed");
    return self->requestElicitation(name,request,signal);
  };
  manager_=Manager::create(std::move(manager));
}
event_loop::Deferred<Json> IndigenousRuntime::requestElicitation(
    const std::string& name,const Json& request,std::stop_token signal){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "MCP elicitation requires owner thread");
  if(inputs_.elicitationRequest&&!signal.stop_requested()&&!lifetime_.stop_requested())
    return inputs_.elicitationRequest(name,request,signal);
  event_loop::Deferred<Json> canceled(inputs_.owner);
  canceled.resolve(Json{{"action","cancel"}});return canceled;
}
event_loop::Deferred<event_loop::Unit> IndigenousRuntime::start(){return manager_->start();}
void IndigenousRuntime::setGadgetServices(Services services){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("MCP tool services require owner");
  inputs_.gadgetServices=std::move(services);
}
event_loop::Deferred<event_loop::Unit> IndigenousRuntime::shutdown(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("MCP shutdown requires owner");
  if(stopped_)return *stopped_;
  stopped_.emplace(inputs_.owner);lifetime_.request_stop();
  auto self=shared_from_this();manager_->shutdown().then([self](const event_loop::Unit*,const std::exception_ptr&){
    self->managerStopped_=true;self->finishShutdown();});
  return *stopped_;
}
void IndigenousRuntime::finishShutdown(){
  if(stopped_&&managerStopped_&&!authJobs_)stopped_->resolve({});
}
auth::Services IndigenousRuntime::authenticationServices(const std::string& name){
  auto services=auth_.services;
  const auto diagnostic=inputs_.diagnostic,url=inputs_.authorizationUrl;
  const auto owner=inputs_.owner.dispatcher();
  services.authorizationUrl=[owner,name,url,diagnostic](const std::string& value){
    owner.postMission([name,url,value,diagnostic]{if(url)url(name,value);else diagnostic(name,"Open this authorization URL: "+value);});};
  return services;
}
} // namespace batbox::mcp::manager
