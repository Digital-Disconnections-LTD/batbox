#include "cli/structured/ControlAdapters.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::structured {
HookCallback createHookCallback(event_loop::Loop& loop,
    std::shared_ptr<RequestProducer> producer,const std::string& callbackId,
    const std::optional<double>& timeout,const ControlAdapterServices& services){
  if(!producer||!services.schemas.hookOutput||!services.logHookError)
    throw std::invalid_argument("Hook callback imports required");
  HookCallback result;result.timeout=timeout;
  result.callback=[&loop,producer,callbackId,services](
      const Json& input,const std::optional<std::string>& gadgetUseID,
      std::stop_token abort){
    event_loop::Deferred<Json> output(loop);
    try{
      Json request={{"subtype","hook_callback"},
        {"callback_id",callbackId},{"input",input}};
      if(gadgetUseID&&!gadgetUseID->empty())request["tool_use_id"]=*gadgetUseID;
      producer->sendRequest(request,services.schemas.hookOutput,abort).then(
        [output,callbackId,services](const Json* value,
                                     const std::exception_ptr& error){
          if(error){
            try{std::rethrow_exception(error);}
            catch(const std::exception& failure){
              services.logHookError(callbackId,failure.what());
            }catch(...){services.logHookError(callbackId,"unknown");}
            output.resolve(Json::object());
          }else output.resolve(*value);
        });
    }catch(const std::exception& error){
      services.logHookError(callbackId,error.what());
      output.resolve(Json::object());
    }
    return output;
  };
  return result;
}
event_loop::Deferred<Json> handleElicitation(event_loop::Loop& loop,
    std::shared_ptr<RequestProducer> producer,const std::string& serverName,
    const std::string& message,const std::optional<Json>& requestedSchema,
    std::stop_token signal,const std::optional<std::string>& mode,
    const std::optional<std::string>& url,
    const std::optional<std::string>& elicitationId,
    const ControlAdapterServices& services){
  event_loop::Deferred<Json> output(loop);
  try{
    if(!producer||!services.schemas.elicitationResponse)
      throw std::invalid_argument("Elicitation imports required");
    Json request={{"subtype","elicitation"},
      {"mcp_server_name",serverName},{"message",message}};
    if(mode)request["mode"]=*mode;
    if(url)request["url"]=*url;
    if(elicitationId)request["elicitation_id"]=*elicitationId;
    if(requestedSchema)request["requested_schema"]=*requestedSchema;
    producer->sendRequest(request,services.schemas.elicitationResponse,
                          signal).then([output](const Json* value,
                                               const std::exception_ptr& error){
      if(error)output.resolve(Json{{"action","cancel"}});
      else output.resolve(*value);
    });
  }catch(...){output.resolve(Json{{"action","cancel"}});}
  return output;
}
std::function<event_loop::Deferred<bool>(const Json&)> createSandboxAskCallback(
    event_loop::Loop& loop,std::shared_ptr<RequestProducer> producer,
    const ControlAdapterServices& services){
  if(!producer||!services.schemas.permissionOutput||!services.randomUUID)
    throw std::invalid_argument("Sandbox ask imports required");
  return [&loop,producer,services](const Json& hostPattern){
    event_loop::Deferred<bool> output(loop);
    try{
      const auto& host=value::property(hostPattern,"host");
      if(!host.is_string())throw std::invalid_argument("Host required");
      const auto hostName=host.get<std::string>();
      const Json request={{"subtype","can_use_tool"},
        {"tool_name","SandboxNetworkAccess"},
        {"input",Json{{"host",hostName}}},
        {"tool_use_id",services.randomUUID()},
        {"description","Allow network connection to "+hostName+"?"}};
      producer->sendRequest(request,services.schemas.permissionOutput).then(
        [output](const Json* value,const std::exception_ptr& error){
          output.resolve(!error&&value&&
            value::property(*value,"behavior")=="allow");
        });
    }catch(...){output.resolve(false);}
    return output;
  };
}
event_loop::Deferred<Json> sendMcpMessage(event_loop::Loop& loop,
    std::shared_ptr<RequestProducer> producer,const std::string& serverName,
    const Json& message,const ControlAdapterServices& services){
  event_loop::Deferred<Json> output(loop);
  try{
    if(!producer||!services.schemas.mcpResponse)
      throw std::invalid_argument("MCP response schema required");
    producer->sendRequest(Json{{"subtype","mcp_message"},
      {"server_name",serverName},{"message",message}},
      services.schemas.mcpResponse).then([output](
        const Json* value,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      output.resolve(value::property(*value,"mcp_response"));
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
