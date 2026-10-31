#include "mcp/manager/Manager.hpp"
#include "application/Async.hpp"
#include "mcp/StdioClient.hpp"
#include <algorithm>

namespace batbox::mcp::manager {
ClientHandlers Manager::handlers(const std::string& name,std::uint64_t generation){
  ClientHandlers result;const auto owner=inputs_.loop.dispatcher();const auto weak=weak_from_this();
  result.closed=[owner,weak,name,generation]{owner.postMission([weak,name,generation]{
    if(auto self=weak.lock())self->onClosed(name,generation);
  });};
  result.needsAuth=[owner,weak,name,generation]{owner.postMission([weak,name,generation]{
    if(auto self=weak.lock()){
      if(self->closing_||self->generations_[name]!=generation)return;
      auto current=self->server(name);current.type="needs-auth";
      self->updateServer({std::move(current),{},{},{}});
    }
  });};
  result.notification=[owner,weak,name,generation](const Json& notification){
    owner.postMission([weak,name,generation,notification]{
      if(auto self=weak.lock())self->changed(name,generation,notification);
    });
  };
  result.request=[owner,weak,name](const Json& request){
    return application::onOwnerAndWait<Json>(owner,[weak,name,request]{
      auto self=weak.lock();if(!self)throw std::runtime_error("MCP manager closed");
      if(self->inputs_.serverRequest)return self->inputs_.serverRequest(name,request);
      const auto& method=value::property(request,"method");
      if(method=="ping")return Json::object();
      if(method=="elicitation/create")return Json{{"action","cancel"}};
      throw ProtocolError(-32601,"Client method unavailable");
    });
  };
  result.requestAsync=[owner,weak,name,generation](const Json& request,
      std::stop_token signal,
      std::function<void(Json,std::exception_ptr)> complete){
    owner.postMission([weak,name,generation,request,signal,
        complete=std::move(complete)]{
      try{
        auto self=weak.lock();
        if(!self||self->closing_||self->generations_[name]!=generation||
            signal.stop_requested()||!self->inputs_.serverRequestAsync){
          complete(Json{{"action","cancel"}},{});return;
        }
        self->inputs_.serverRequestAsync(name,request,signal).then(
          [complete](const Json* value,const std::exception_ptr& error){
            if(error)complete(Json(),error);
            else complete(value?*value:Json{{"action","cancel"}},{});
          });
      }catch(...){complete(Json(),std::current_exception());}
    });
  };
  return result;
}
void Manager::onClosed(const std::string& name,std::uint64_t generation){
  requireOwner();
  if(closing_||generations_[name]!=generation||inputs_.isDisabled(name))return;
  auto current=server(name);
  const auto& type=value::property(current.config,"type");
  if(type.is_null()||type=="stdio"||type=="sdk"){
    current.type="failed";updateServer({std::move(current),{},{},{}});return;
  }
  cancelReconnect(name);automaticReconnect(name,generation,1);
}
void Manager::automaticReconnect(const std::string& name,std::uint64_t generation,std::size_t attempt){
  requireOwner();
  if(closing_||generations_[name]!=generation||inputs_.isDisabled(name))return;
  auto current=server(name);current.type="pending";current.reconnectAttempt=attempt;current.maxReconnectAttempts=5;
  if(current.close)dispose(current);
  current.close={};current.owner.reset();current.connection.reset();
  updateServer({current,{},{},{}});
  auto self=shared_from_this();
  beginJob();
  const auto finished=[self,name,generation,attempt](const ServerResult* value,const std::exception_ptr& error){
    if(self->closing_||self->generations_[name]!=generation||self->inputs_.isDisabled(name)){
      if(value)self->dispose(*value);
      self->endJob();
      return;
    }
    if(value&&value->type=="connected"){
      self->onConnectionAttempt(*value,generation);self->endJob();return;
    }
    if(attempt==5){
      if(value)self->onConnectionAttempt(*value,generation);
      else{auto failed=self->server(name);failed.type="failed";
        self->updateServer({std::move(failed),{},{},{}});}
      self->cancelReconnect(name);self->endJob();return;
    }
    if(error)self->inputs_.diagnostic(name,"MCP reconnection attempt failed");
    const auto delay=std::min(1000.0*static_cast<double>(std::uint64_t{1}<<(attempt-1)),30000.0);
    self->reconnectTimers_[name]=self->inputs_.loop.setTimeout(delay,[self,name,generation,attempt]{
      self->reconnectTimers_.erase(name);self->automaticReconnect(name,generation,attempt+1);
    });
    self->endJob();
  };
  try{inputs_.connect(name,current.config,handlers(name,generation)).then(finished);}
  catch(...){finished(nullptr,std::current_exception());}
}
void Manager::changed(const std::string& name,std::uint64_t generation,const Json& notification){
  requireOwner();if(closing_||generations_[name]!=generation)return;
  if(inputs_.notification)inputs_.notification(name,notification);
  const auto& method=value::property(notification,"method");
  if(!inputs_.refresh||!method.is_string())return;
  std::string kind;
  if(method=="notifications/tools/list_changed")kind="tools";
  else if(method=="notifications/prompts/list_changed")kind="commands";
  else if(method=="notifications/resources/list_changed")kind="resources";
  if(kind.empty())return;
  auto self=shared_from_this();
  beginJob();
  try{inputs_.refresh(server(name),kind).then([self,name,generation,kind](
      const ServerResult* value,const std::exception_ptr& error){
    if(error||!value||self->closing_||self->generations_[name]!=generation){self->endJob();return;}
    Update update;update.client=self->server(name);
    if(kind=="tools")update.gadgets=value->gadgets;
    else if(kind=="commands")update.commands=value->commands;
    else update.resources=value->resources;
    self->updateServer(std::move(update));
    self->endJob();
  });}catch(...){endJob();inputs_.diagnostic(name,"MCP list refresh failed");}
}
} // namespace batbox::mcp::manager
