#include "application_hooks/Provider.hpp"
#include "instructions/hooks/runtime/NotificationHooks.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::application_hooks {
namespace {
std::string field(const Json& value,const char* key){
  const auto& found=value::property(value,key);
  return found.is_string()?found.get<std::string>():std::string{};
}
Json response(const hooks::ElicitationResponse& value){
  Json result={{"action",value.action}};
  if(value.content)result["content"]=*value.content;
  return result;
}
}
event_loop::Deferred<Json> Provider::elicitationBefore(
    const std::string& server,const Json& params,std::stop_token signal){
  event_loop::Deferred<Json> output(owner_);
  try{
    auto services=hooks::bindElicitationHookServices(owner_,baseServices_,
      executor_,{},inputs_.defaultTimeoutMs);
    hooks::ElicitationRequest request;
    request.serverName=server;request.message=field(params,"message");
    request.mode=field(params,"mode").empty()?"form":field(params,"mode");
    if(params.contains("requestedSchema"))request.requestedSchema=params.at("requestedSchema");
    if(params.contains("url"))request.url=field(params,"url");
    if(params.contains("elicitationId"))request.elicitationId=field(params,"elicitationId");
    request.signal=signal;
    hooks::executeElicitationHooks(owner_,services,request).then(
      [output](const hooks::ElicitationHookOutput* value,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        if(value&&value->blockingError)output.resolve(Json{{"action","decline"}});
        else if(value&&value->response)output.resolve(response(*value->response));
        else output.resolve(Json());
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<Json> Provider::elicitationAfter(
    const std::string& server,const Json& params,const Json& raw,
    std::stop_token signal){
  event_loop::Deferred<Json> output(owner_);
  try{
    auto services=hooks::bindElicitationHookServices(owner_,baseServices_,
      executor_,{},inputs_.defaultTimeoutMs);
    hooks::ElicitationResultRequest request;
    request.serverName=server;request.action=field(raw,"action");
    if(raw.contains("content"))request.content=raw.at("content");
    request.mode=field(params,"mode").empty()?"form":field(params,"mode");
    if(params.contains("elicitationId"))request.elicitationId=field(params,"elicitationId");
    request.signal=signal;
    hooks::executeElicitationResultHooks(owner_,services,request).then(
      [self=shared_from_this(),output,raw,server](const hooks::ElicitationHookOutput* value,
          const std::exception_ptr& error){
        Json final=raw;
        if(error){
          self->notifyElicitationResponse(server,field(raw,"action"));
          output.reject(error);return;
        }
        if(value&&value->blockingError)final={{"action","decline"}};
        else if(value&&value->response){
          Json final=response(*value->response);
          if(!final.contains("content")&&raw.contains("content"))
            final["content"]=raw.at("content");
          self->notifyElicitationResponse(server,field(final,"action"));
          output.resolve(std::move(final));return;
        }
        self->notifyElicitationResponse(server,field(final,"action"));
        output.resolve(std::move(final));
      });
  }catch(...){
    notifyElicitationResponse(server,field(raw,"action"));
    output.reject(std::current_exception());
  }
  return output;
}
void Provider::notifyElicitationResponse(const std::string& server,
    const std::string& action){
  try{
    auto services=hooks::bindNotificationHookServices(owner_,baseServices_,
      executor_,inputs_.defaultTimeoutMs);
    hooks::executeNotificationHooks(owner_,services,
      "Elicitation response for server \""+server+"\": "+action,
      std::nullopt,"elicitation_response").then(
      [log=inputs_.log](const event_loop::Unit*,const std::exception_ptr& error){
        if(error&&log)log("Elicitation response notification hook failed");
      });
  }catch(...){if(inputs_.log)inputs_.log(
    "Elicitation response notification hook failed");}
}
void Provider::elicitationComplete(const std::string& server,
    const std::string& elicitationId){
  try{
    auto services=hooks::bindNotificationHookServices(owner_,baseServices_,
      executor_,inputs_.defaultTimeoutMs);
    hooks::executeNotificationHooks(owner_,services,
      "MCP server \""+server+"\" confirmed elicitation "+elicitationId+
        " complete",std::nullopt,"elicitation_complete").then(
      [log=inputs_.log](const event_loop::Unit*,const std::exception_ptr& error){
        if(error&&log)log("Elicitation completion notification hook failed");
      });
  }catch(...){if(inputs_.log)inputs_.log(
    "Elicitation completion notification hook failed");}
}
} // namespace batbox::application_hooks
