#include "mcp/remote/RemoteClient.hpp"
#include "mcp/remote/EventStream.hpp"
#include "mcp/StdioClient.hpp"
#include "connectors/Abort.hpp"
#include <stdexcept>

namespace batbox::mcp {
void RemoteClient::failAll(std::exception_ptr error){
  std::vector<std::shared_ptr<Pending>> waiting;
  {std::lock_guard lock(pendingMutex_);for(auto& [_,value]:pending_)waiting.push_back(value);pending_.clear();}
  for(const auto& value:waiting){
    std::lock_guard lock(value->mutex);value->error=error;value->complete=true;value->ready.notify_all();
  }
}
void RemoteClient::receive(const Json& message){
  if(!message.is_object())return;
  const auto& method=value::property(message,"method");
  if(method.is_string()){
    const auto& id=value::property(message,"id");
    if(!id.is_null()&&!id.is_discarded()){
      Json reply={{"jsonrpc","2.0"},{"id",id}};
      if(method=="elicitation/create"&&config_.handlers.requestAsync){
        auto weak=weak_from_this();
        auto answered=std::make_shared<std::atomic_bool>(false);
        const auto complete=[weak,id,answered](Json result,
            std::exception_ptr error){
          if(answered->exchange(true))return;
          auto self=weak.lock();if(!self||self->closing_)return;
          Json response={{"jsonrpc","2.0"},{"id",id}};
          if(error)try{std::rethrow_exception(error);}
            catch(const ProtocolError& issue){response["error"]={{"code",issue.code},
              {"message",issue.what()}};}
            catch(...){response["error"]={{"code",-32603},
              {"message","Client request failed"}};}
          else response["result"]=std::move(result);
          std::thread([self,response=std::move(response)]{
            try{self->send(response,self->config_.lifecycleSignal,
              self->config_.requestTimeout);}catch(...){}
          }).detach();
        };
        try{config_.handlers.requestAsync(message,
          requestLifetime_.get_token(),complete);}
        catch(...){complete(Json(),std::current_exception());}
        return;
      }
      try{
        if(config_.handlers.request)reply["result"]=config_.handlers.request(message);
        else if(method=="ping")reply["result"]=Json::object();
        else if(method=="roots/list")reply["result"]={{"roots",Json::array({{{"uri","file://"+config_.originalCwd}}})}};
        else if(method=="elicitation/create")reply["result"]={{"action","cancel"}};
        else throw ProtocolError(-32601,"Client method unavailable");
      }catch(const ProtocolError& error){reply["error"]={{"code",error.code},{"message",error.what()}};}
      catch(const std::exception&){reply["error"]={{"code",-32603},{"message","Client request failed"}};}
      send(reply,{},config_.requestTimeout);return;
    }
    if(method=="notifications/progress"){
      const auto& params=value::property(message,"params");const auto& token=value::property(params,"progressToken");
      if(token.is_number_integer()){
        std::shared_ptr<Pending> pending;
        {std::lock_guard lock(pendingMutex_);const auto found=pending_.find(token.get<std::int64_t>());
          if(found!=pending_.end())pending=found->second;}
        if(pending&&pending->progress){
          Json value={{"type","mcp_progress"},{"status","progress"},{"serverName",config_.name},{"toolName",pending->gadget}};
          for(const auto& [from,to]:{std::pair{"progress","progress"},std::pair{"total","total"},
              std::pair{"message","progressMessage"}})
            if(params.contains(from))value[to]=params.at(from);
          pending->progress(value);
        }
      }
    }else if(config_.handlers.notification)config_.handlers.notification(message);
    return;
  }
  const auto& id=value::property(message,"id");if(!id.is_number_integer())return;
  std::shared_ptr<Pending> pending;
  {std::lock_guard lock(pendingMutex_);const auto found=pending_.find(id.get<std::int64_t>());
    if(found!=pending_.end()){pending=found->second;pending_.erase(found);}}
  if(!pending)return;
  std::lock_guard lock(pending->mutex);
  if(message.contains("error")){
    const auto& error=message.at("error");
    pending->error=std::make_exception_ptr(ProtocolError(error.value("code",-32000),
      error.value("message",std::string("MCP protocol error")),value::property(error,"data")));
  }else pending->result=value::property(message,"result");
  pending->complete=true;pending->ready.notify_all();
}
void RemoteClient::listen(std::stop_token signal){
  std::stop_source stopped;
  std::stop_callback caller(signal,[&]{stopped.request_stop();});
  std::stop_callback lifetime(config_.lifecycleSignal,[&]{stopped.request_stop();});
  signal=stopped.get_token();
  try{
    http::Request request;request.url=config_.url;request.signal=signal;
    request.headers=headers("text/event-stream");request.timeout=std::chrono::milliseconds(0);
    {std::lock_guard lock(stateMutex_);if(!lastEventId_.empty())request.headers["Last-Event-ID"]=lastEventId_;}
    EventStream stream;
    request.onData=[this,&stream](std::string_view bytes){
      stream.feed(bytes,[this](const RemoteEvent& event){
        if(event.id){std::lock_guard lock(stateMutex_);lastEventId_=*event.id;}
        if(event.event=="endpoint"&&config_.type!="http"){
          std::lock_guard lock(stateMutex_);endpoint_=http::resolveUrl(config_.url,event.data);endpointReady_.notify_all();
        }else if(event.event=="message"||event.event.empty()){
          if(!event.data.empty())receive(Json::parse(event.data));
        }
      });return true;
    };
    const auto response=config_.fetch(request);
    if(response.status==405&&config_.type=="http")return;
    if(response.status==401||response.status==403){
      if(config_.handlers.needsAuth)config_.handlers.needsAuth();
    }
    if(!signal.stop_requested())closeNotification();
  }catch(...){if(!signal.stop_requested())closeNotification();}
}
std::shared_ptr<Connection> makeRemoteConnection(const std::shared_ptr<RemoteClient>& client,
    const std::string& name,const std::string& transport,bool skipPrefix){
  auto connection=std::make_shared<Connection>();connection->name=name;
  connection->transportType=transport;connection->connected=client->connected();
  connection->hasGadgetCapability=client->capabilities().contains("tools");
  connection->skipPrefix=skipPrefix;connection->instructions=client->instructions();
  connection->capabilities=client->capabilities();connection->serverInfo=client->serverInfo();
  connection->request=[client](const std::string& method,const Json& params,std::stop_token signal){
    return client->invoke(method,params,signal);};
  connection->listGadgets=[client](std::stop_token signal){return client->listGadgets(signal);};
  connection->callGadget=[client](const std::string& name,const Json& values,std::stop_token signal,
      const std::function<void(const Json&)>& progress){return client->callGadget(name,values,signal,progress);};
  connection->ensureConnected=[client]{client->ensureConnected();};
  connection->clearServerStash=[client]{client->expireFlight();};
  return connection;
}
} // namespace batbox::mcp
