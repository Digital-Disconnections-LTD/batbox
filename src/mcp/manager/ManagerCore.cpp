#include "mcp/manager/Manager.hpp"
#include "application/Async.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::mcp::manager {
namespace {
Json clientJson(const ServerResult& value){
  Json result={{"name",value.name},{"type",value.type},{"config",value.config}};
  if(!value.error.empty())result["error"]=value.error;
  if(value.connection){
    result["capabilities"]=value.connection->capabilities;
    result["serverInfo"]=value.connection->serverInfo;
    if(!value.connection->instructions.empty())result["instructions"]=value.connection->instructions;
  }
  if(value.reconnectAttempt){result["reconnectAttempt"]=value.reconnectAttempt;
    result["maxReconnectAttempts"]=value.maxReconnectAttempts;}
  return result;
}
}
Manager::Manager(Inputs inputs):inputs_(std::move(inputs)){
  requireOwner();
  if(!inputs_.connect||!inputs_.isDisabled||!inputs_.setEnabled||!inputs_.changed)
    throw std::invalid_argument("MCP connection manager bindings required");
  if(!inputs_.diagnostic)inputs_.diagnostic=[](const std::string&,const std::string&){};
}
std::shared_ptr<Manager> Manager::create(Inputs inputs){
  return std::shared_ptr<Manager>(new Manager(std::move(inputs)));
}
Manager::~Manager(){
  flushTimer_.cancel();for(auto& [_,timer]:reconnectTimers_)timer.cancel();
  for(auto& value:servers_)if(value.close)try{value.close();}catch(...){}
}
void Manager::requireOwner()const{
  if(!inputs_.loop.isOwnerThread())throw std::logic_error("MCP manager requires owner thread");
}
ServerResult& Manager::server(const std::string& name){
  const auto found=std::find_if(servers_.begin(),servers_.end(),[&](const auto& value){return value.name==name;});
  if(found==servers_.end())throw std::runtime_error("MCP server "+name+" not found");
  return *found;
}
void Manager::dispose(ServerResult value){
  if(!value.close)return;
  auto close=std::move(value.close);
  beginJob();
  auto self=shared_from_this();
  application::runInWorker<event_loop::Unit>(inputs_.loop,[close=std::move(close)]{
    close();return event_loop::Unit{};
  }).then([self](const event_loop::Unit*,const std::exception_ptr&){self->endJob();});
}
void Manager::beginJob(){requireOwner();++activeJobs_;}
void Manager::endJob(){
  requireOwner();if(!activeJobs_)throw std::logic_error("MCP job accounting underflow");
  --activeJobs_;
  if(closing_&&!activeJobs_&&shutdownResult_)shutdownResult_->resolve({});
}
void Manager::cancelReconnect(const std::string& name){
  const auto found=reconnectTimers_.find(name);
  if(found!=reconnectTimers_.end()){found->second.cancel();reconnectTimers_.erase(found);}
}
void Manager::updateServer(Update update){
  requireOwner();if(closing_){dispose(std::move(update.client));return;}
  pending_.push_back(std::move(update));
  if(!flushTimer_.valid()){
    const auto weak=weak_from_this();flushTimer_=inputs_.loop.setTimeout(16,[weak]{
      if(auto self=weak.lock()){self->flushTimer_={};self->flush();}
    });
  }
}
void Manager::flush(){
  requireOwner();flushTimer_.cancel();flushTimer_={};
  auto updates=std::move(pending_);pending_.clear();
  if(updates.empty())return;
  for(auto& update:updates){
    auto found=std::find_if(servers_.begin(),servers_.end(),[&](const auto& value){return value.name==update.client.name;});
    if(found==servers_.end()){servers_.push_back({});found=std::prev(servers_.end());}
    const bool clear=update.client.type=="disabled"||update.client.type=="failed";
    if(clear){
      if(!update.gadgets)update.gadgets=std::vector<gadgets::Gadget>{};
      if(!update.commands)update.commands=Json::array();
      if(!update.resources)update.resources=Json::array();
    }
    auto gadgets=update.gadgets?std::move(*update.gadgets):std::move(found->gadgets);
    auto commands=update.commands?std::move(*update.commands):std::move(found->commands);
    auto resources=update.resources?std::move(*update.resources):std::move(found->resources);
    *found=std::move(update.client);found->gadgets=std::move(gadgets);
    found->commands=std::move(commands);found->resources=std::move(resources);
  }
  inputs_.changed(servers_);
}
void Manager::flushPending(){requireOwner();flush();}
void Manager::onConnectionAttempt(ServerResult result,std::uint64_t generation){
  requireOwner();
  if(closing_||generations_[result.name]!=generation){dispose(std::move(result));return;}
  const auto name=result.name;
  Update update;update.gadgets=std::move(result.gadgets);update.commands=std::move(result.commands);
  update.resources=std::move(result.resources);update.client=std::move(result);
  updateServer(std::move(update));
  cancelReconnect(name);
}
std::vector<ServerResult> Manager::snapshot()const{requireOwner();return servers_;}
Json Manager::clients()const{
  requireOwner();Json result=Json::array();for(const auto& value:servers_)result.push_back(clientJson(value));return result;
}
Json Manager::resources()const{
  requireOwner();Json result=Json::object();
  for(const auto& value:servers_)if(value.resources.is_array()&&!value.resources.empty())result[value.name]=value.resources;
  return result;
}
std::vector<gadgets::Gadget> Manager::gadgets()const{
  requireOwner();std::vector<gadgets::Gadget> result;
  for(const auto& value:servers_)result.insert(result.end(),value.gadgets.begin(),value.gadgets.end());
  return result;
}
event_loop::Deferred<event_loop::Unit> Manager::shutdown(){
  requireOwner();if(shutdownResult_)return *shutdownResult_;
  shutdownResult_.emplace(inputs_.loop);closing_=true;
  if(inputs_.cancel)inputs_.cancel();
  flushTimer_.cancel();flushTimer_={};
  for(auto& [_,timer]:reconnectTimers_)timer.cancel();
  reconnectTimers_.clear();
  for(auto& value:pending_)dispose(std::move(value.client));
  pending_.clear();
  for(auto& value:servers_){++generations_[value.name];dispose(std::move(value));}
  servers_.clear();
  if(!activeJobs_)shutdownResult_->resolve({});
  return *shutdownResult_;
}
} // namespace batbox::mcp::manager
