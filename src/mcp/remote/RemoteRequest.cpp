#include "mcp/remote/RemoteClient.hpp"
#include "mcp/remote/EventStream.hpp"
#include "mcp/StdioClient.hpp"
#include "mcp/auth/Discovery.hpp"
#include "connectors/Abort.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace batbox::mcp {
void RemoteClient::send(const Json& message,std::stop_token signal,
    std::chrono::milliseconds timeout,bool retryAuth){
  std::stop_source stopped;
  std::stop_callback caller(signal,[&]{stopped.request_stop();});
  std::stop_callback lifetime(config_.lifecycleSignal,[&]{stopped.request_stop();});
  signal=stopped.get_token();
  checkConnectorAbort(signal);
  http::Request request;request.method="POST";
  {std::lock_guard lock(stateMutex_);request.url=endpoint_;}
  request.headers=headers("application/json, text/event-stream");
  request.headers["Content-Type"]="application/json";
  request.body=value::stringify(message);request.signal=signal;request.timeout=timeout;
  request.connectTimeout=config_.connectTimeout;
  std::string jsonBody;EventStream events;bool sse=false,decided=false;
  std::optional<std::int64_t> ownId;
  if(value::property(message,"id").is_number_integer())ownId=message.at("id").get<std::int64_t>();
  const auto onEvent=[&](const RemoteEvent& event){
    if(event.id){std::lock_guard lock(stateMutex_);lastEventId_=*event.id;}
    if(event.event=="message"||event.event.empty()){
      if(!event.data.empty())receive(Json::parse(event.data));
    }
  };
  request.onData=[&](std::string_view bytes){
    if(!decided){
      jsonBody.append(bytes);
      const auto first=jsonBody.find_first_not_of(" \t\r\n");
      if(first==std::string::npos)return true;
      decided=true;sse=jsonBody[first]!='{'&&jsonBody[first]!='[';
      if(sse){events.feed(jsonBody,onEvent);jsonBody.clear();}
    }else if(sse)events.feed(bytes,onEvent);
    else jsonBody.append(bytes);
    if(ownId&&sse){
      std::lock_guard lock(pendingMutex_);
      return pending_.contains(*ownId);
    }
    return true;
  };
  http::Response response;
  try{response=config_.fetch(request);}
  catch(...){if(signal.stop_requested())throw ConnectorAbortError();throw;}
  if(response.status==401||response.status==403){
    if(config_.auth){
      const auto challenge=response.header("www-authenticate");
      const auto scope=challenge.find("scope=\"");
      if(scope!=std::string::npos){
        const auto end=challenge.find('"',scope+7);
        if(end!=std::string::npos)config_.auth->markStepUpPending(challenge.substr(scope+7,end-scope-7));
      }
      if(retryAuth&&response.status==401){
        bool refreshed=false;
        try{refreshed=!config_.auth->refreshRejectedAccessToken().is_null();}
        catch(const std::exception&){}
        if(refreshed){send(message,signal,timeout,false);return;}
      }
      // The noninteractive SDK authorization pass saves discovery, client
      // registration and step-up scope. Opening a browser is an explicit UI flow.
      if(retryAuth){
        try{
          const auto challenge=response.header("www-authenticate");
          std::optional<std::string> metadata;
          const auto at=challenge.find("resource_metadata=\"");
          if(at!=std::string::npos){const auto begin=at+19;const auto end=challenge.find('"',begin);
            if(end!=std::string::npos)metadata=challenge.substr(begin,end-begin);}
          auth::OAuthFlight flight(*config_.auth,config_.fetch,signal);
          (void)flight.begin({},metadata);
        }catch(const std::exception&){}
      }
    }
    if(config_.handlers.needsAuth)config_.handlers.needsAuth();
    throw UnauthorizedError("MCP server requires authorization: "+config_.name);
  }
  bool hasFlight;
  {std::lock_guard lock(stateMutex_);hasFlight=!flightId_.empty();}
  if(response.status==404&&hasFlight){
    expireFlight();
    throw HttpStatusError(404,"MCP session expired {\"code\":-32001}");
  }
  if(!response.ok())throw HttpStatusError(response.status,
    "MCP request failed with HTTP "+std::to_string(response.status));
  const auto flight=response.header("mcp-session-id");
  if(!flight.empty()){std::lock_guard lock(stateMutex_);flightId_=flight;}
  if(sse)events.flush(onEvent);
  else if(!value::trim(jsonBody).empty()){
    const auto parsed=Json::parse(jsonBody);
    if(parsed.is_array())for(const auto& reply:parsed)receive(reply);
    else receive(parsed);
  }else if(!response.body.empty())receive(response.json());
}
Json RemoteClient::request(const std::string& method,const Json& values,std::stop_token signal,
    const std::function<void(const Json&)>& progress,std::chrono::milliseconds timeout){
  checkConnectorAbort(signal);
  checkConnectorAbort(config_.lifecycleSignal);
  const auto id=++nextId_;
  auto pending=std::make_shared<Pending>();pending->method=method;pending->progress=progress;
  const auto& gadget=value::property(values,"name");if(gadget.is_string())pending->gadget=gadget.get<std::string>();
  {std::lock_guard lock(pendingMutex_);pending_[id]=pending;}
  const auto erase=[this,id]{std::lock_guard lock(pendingMutex_);pending_.erase(id);};
  const auto deadline=std::chrono::steady_clock::now()+timeout;
  auto params=values;
  if(progress)params["_meta"]["progressToken"]=id;
  try{
    send({{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",params}},signal,timeout);
    for(;;){
      std::unique_lock lock(pending->mutex);
      if(pending->ready.wait_for(lock,std::chrono::milliseconds(50),[&]{return pending->complete;})){
        if(pending->error)std::rethrow_exception(pending->error);
        return pending->result;
      }
      lock.unlock();
      if(signal.stop_requested()||config_.lifecycleSignal.stop_requested()||std::chrono::steady_clock::now()>=deadline){
        try{send({{"jsonrpc","2.0"},{"method","notifications/cancelled"},
          {"params",{{"requestId",id},{"reason",signal.stop_requested()?"Operation aborted":"Request timed out"}}}},
          {},std::chrono::milliseconds(1000));}catch(...){}
        if(signal.stop_requested()||config_.lifecycleSignal.stop_requested())throw ConnectorAbortError();
        throw std::runtime_error("MCP server \""+config_.name+"\" tool \""+pending->gadget+
          "\" timed out after "+std::to_string(timeout.count()/1000)+"s");
      }
    }
  }catch(...){erase();throw;}
}
Json RemoteClient::invoke(const std::string& method,const Json& params,std::stop_token signal,
    const std::function<void(const Json&)>& progress){
  ensureConnected();
  return request(method,params,signal,progress,
    method=="tools/call"?config_.gadgetTimeout:config_.requestTimeout);
}
Json RemoteClient::listGadgets(std::stop_token signal){return invoke("tools/list",Json::object(),signal);}
GadgetCallResult RemoteClient::callGadget(const std::string& name,const Json& args,
    std::stop_token signal,const std::function<void(const Json&)>& progress){
  const auto result=invoke("tools/call",{{"name",name},{"arguments",args}},signal,progress);
  if(!result.is_object())throw std::runtime_error("Invalid MCP tool response");
  if(value::truthy(value::property(result,"isError"))){
    std::string message="Unknown error";const auto& content=value::property(result,"content");
    if(content.is_array()&&!content.empty()&&value::property(content.front(),"text").is_string())
      message=content.front().at("text").get<std::string>();
    else if(result.contains("error"))message=jsStringValue(result.at("error"));
    throw GadgetCallError(message,value::property(result,"_meta"));
  }
  return {value::property(result,"content"),value::property(result,"_meta"),
    value::property(result,"structuredContent")};
}
} // namespace batbox::mcp
