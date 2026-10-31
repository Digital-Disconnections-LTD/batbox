#include "mcp/manager/Manager.hpp"
#include "application/Async.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::mcp::manager {
event_loop::Deferred<event_loop::Unit> Manager::start(){
  requireOwner();started_=true;return configure(inputs_.configurations);
}
event_loop::Deferred<event_loop::Unit> Manager::configure(Json configurations){
  requireOwner();
  event_loop::Deferred<event_loop::Unit> ready(inputs_.loop);
  if(!configurations.is_object())throw std::invalid_argument("MCP server configurations must be an object");
  if(closing_){ready.reject(std::make_exception_ptr(std::runtime_error("MCP manager closed")));return ready;}
  flush();
  for(auto it=servers_.begin();it!=servers_.end();){
    if(!configurations.contains(it->name)){
      cancelReconnect(it->name);++generations_[it->name];dispose(std::move(*it));it=servers_.erase(it);
    }else ++it;
  }
  inputs_.configurations=configurations;
  auto count=std::make_shared<std::size_t>(0);
  std::vector<std::string> connect;
  for(auto it=configurations.begin();it!=configurations.end();++it){
    const auto existing=std::find_if(servers_.begin(),servers_.end(),[&](const auto& value){return value.name==it.key();});
    if(existing!=servers_.end()&&existing->config==it.value()&&existing->type!="failed")continue;
    if(existing==servers_.end()){
      ServerResult pending;pending.name=it.key();pending.config=it.value();
      pending.type=inputs_.isDisabled(it.key())?"disabled":"pending";servers_.push_back(std::move(pending));
    }else{
      auto old=*existing;dispose(std::move(old));existing->config=it.value();
    }
    if(!inputs_.isDisabled(it.key()))connect.push_back(it.key());
  }
  inputs_.changed(servers_);
  if(connect.empty()){ready.resolve({});return ready;}
  *count=connect.size();
  auto self=shared_from_this();
  for(const auto& name:connect)reconnect(name).then([self,ready,count](const ServerResult*,const std::exception_ptr&){
    if(--*count==0){self->flush();ready.resolve({});}
  });
  return ready;
}
event_loop::Deferred<ServerResult> Manager::reconnect(const std::string& name){
  requireOwner();cancelReconnect(name);
  if(closing_)throw std::runtime_error("MCP manager closed");
  auto& current=server(name);
  const auto generation=++generations_[name];
  if(current.close)dispose(current);
  current.close={};current.owner.reset();current.connection.reset();
  ServerResult pending;pending.name=name;pending.config=current.config;pending.type="pending";
  updateServer({pending,{},{},{}});
  event_loop::Deferred<ServerResult> output(inputs_.loop);
  beginJob();
  try{
    auto self=shared_from_this();
    inputs_.connect(name,current.config,handlers(name,generation)).then(
      [self,output,name,generation,config=current.config](const ServerResult* value,const std::exception_ptr& error){
        ServerResult result;
        if(value)result=*value;
        else{result.name=name;result.config=config;result.type="failed";
          try{if(error)std::rethrow_exception(error);}
          catch(const std::exception& failure){result.error=failure.what();}
          catch(...){result.error="MCP connection failed";}}
        self->onConnectionAttempt(result,generation);output.resolve(std::move(result));self->endJob();
      });
  }catch(...){output.reject(std::current_exception());endJob();}
  return output;
}
event_loop::Deferred<event_loop::Unit> Manager::toggle(const std::string& name){
  requireOwner();auto& current=server(name);
  event_loop::Deferred<event_loop::Unit> output(inputs_.loop);
  if(current.type!="disabled"){
    cancelReconnect(name);
    inputs_.setEnabled(name,false);
    ++generations_[name];
    if(current.type=="connected")dispose(current);
    ServerResult disabled;disabled.name=name;disabled.config=current.config;disabled.type="disabled";
    updateServer({std::move(disabled),{},{},{}});output.resolve({});
  }else{
    inputs_.setEnabled(name,true);
    reconnect(name).then([output](const ServerResult*,const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve({});
    });
  }
  return output;
}
event_loop::Deferred<Json> Manager::invoke(const std::string& name,const std::string& method,
    const Json& params,std::stop_token signal){
  requireOwner();const auto client=server(name).connection;
  if(closing_)throw std::runtime_error("MCP manager closed");
  if(!client||!client->request)throw std::runtime_error("MCP server is not connected: "+name);
  beginJob();auto self=shared_from_this();
  auto result=application::runInWorker<Json>(inputs_.loop,[client,method,params,signal]{
    if(client->ensureConnected)client->ensureConnected();
    return client->request(method,params,signal);
  });
  result.then([self](const Json*,const std::exception_ptr&){self->endJob();});
  return result;
}
} // namespace batbox::mcp::manager
