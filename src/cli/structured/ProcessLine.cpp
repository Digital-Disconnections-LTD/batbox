#include "cli/structured/ProcessLine.hpp"
#include "cli/structured/ControlMessageCompat.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::structured {
namespace {
const Json& propertyOf(const Json& value,std::string_view key){
  if(value.is_null())throw std::runtime_error("Cannot read property of null");
  return value::property(value,key);
}
std::string textValue(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array())return "";
  return "[object Object]";
}
std::string join(const std::vector<std::string>& values){
  std::string out;
  for(const auto& value:values){if(!out.empty())out+=", ";out+=value;}
  return out;
}
void settleFatal(const std::string& line,const std::string& reason,
                 const ProcessLineServices& services,
                 const event_loop::Deferred<std::optional<Json>>& output){
  const auto text="Error parsing streaming input line: "+line+": "+reason;
  try{
    if(!services.fatalInput)throw std::invalid_argument("Fatal input handler required");
    services.fatalInput(text);
    output.reject(std::make_exception_ptr(std::runtime_error(text)));
  }catch(...){output.reject(std::current_exception());}
}
}
event_loop::Deferred<std::optional<Json>> processLine(
    event_loop::Loop& loop,const std::string& line,ControlResponses& control,
    const ProcessLineServices& services){
  event_loop::Deferred<std::optional<Json>> output(loop);
  if(line.empty()){output.resolve({});return output;}
  try{
    Json message=value::parse(line);
    normalizeControlMessageKeys(message);
    const auto& type=propertyOf(message,"type");
    if(type=="keep_alive"){output.resolve({});return output;}
    if(type=="update_environment_variables"){
      if(!services.setEnvironment||!services.logForDebugging)
        throw std::invalid_argument("Environment update imports required");
      const auto& variables=propertyOf(message,"variables");
      if(variables.is_null())throw std::runtime_error("Missing variables");
      const auto keys=value::objectKeys(variables);
      for(const auto& key:keys)
        services.setEnvironment(key,textValue(propertyOf(variables,key)));
      services.logForDebugging(
        "[structuredIO] applied update_environment_variables: "+join(keys),
        nullptr);
      output.resolve({});return output;
    }
    if(type=="control_response"){
      if(!services.notifyCommandLifecycle)
        throw std::invalid_argument("Lifecycle callback required");
      const auto& uuid=propertyOf(message,"uuid");
      if(uuid.is_string()&&value::truthy(uuid))
        services.notifyCommandLifecycle(uuid.get<std::string>(),"completed");
      const auto& envelope=propertyOf(message,"response");
      const auto& id=propertyOf(envelope,"request_id");
      if(!id.is_string())throw std::runtime_error("Invalid request ID");
      const auto requestId=id.get<std::string>();
      if(!control.hasPendingRequest(requestId)){
        const auto& responsePayload=propertyOf(envelope,"subtype")=="success"
          ?propertyOf(envelope,"response"):Json();
        const auto& gadgetUseID=value::property(responsePayload,"toolUseID");
        if(gadgetUseID.is_string()&&
           control.hasResolvedGadgetUseId(gadgetUseID.get<std::string>())){
          if(!services.logForDebugging)
            throw std::invalid_argument("Debug callback required");
          services.logForDebugging(
            "Ignoring duplicate control_response for already-resolved toolUseID="+
            gadgetUseID.get<std::string>()+" request_id="+requestId,nullptr);
          output.resolve({});return output;
        }
        if(services.unexpectedResponseCallback){
          services.unexpectedResponseCallback(message).then(
            [output,line,services](const event_loop::Unit*,
                                   const std::exception_ptr& error){
              if(error){
                try{std::rethrow_exception(error);}
                catch(const std::exception& failure){
                  settleFatal(line,failure.what(),services,output);
                }catch(...){settleFatal(line,"unknown",services,output);}
              }else output.resolve({});
            });
        }else output.resolve({});
        return output;
      }
      auto pending=control.takePendingRequest(requestId);
      if(!pending)throw std::runtime_error("Pending request disappeared");
      if(value::property(value::property(pending->request,"request"),"subtype")==
         "can_use_tool"&&services.onControlRequestResolved)
        services.onControlRequestResolved(requestId);
      if(propertyOf(envelope,"subtype")=="error"){
        pending->reject(textValue(propertyOf(envelope,"error")));
        output.resolve({});return output;
      }
      const auto& result=propertyOf(envelope,"response");
      if(pending->parseResponse){
        try{pending->resolve(pending->parseResponse(result));}
        catch(const std::exception& error){pending->reject(error.what());}
      }else pending->resolve(Json::object());
      output.resolve(services.replayUserMessages?
        std::optional<Json>(std::move(message)):std::optional<Json>{});
      return output;
    }
    if(type!="user"&&type!="control_request"&&type!="assistant"&&
       type!="system"){
      if(!services.logForDebugging)
        throw std::invalid_argument("Debug callback required");
      services.logForDebugging("Ignoring unknown message type: "+
                               textValue(type),Json{{"level","warn"}});
      output.resolve({});return output;
    }
    if(type=="control_request"){
      if(!value::truthy(propertyOf(message,"request")))
        throw std::runtime_error("Error: Missing request on control_request");
      output.resolve(std::move(message));return output;
    }
    if(type=="assistant"||type=="system"){
      output.resolve(std::move(message));return output;
    }
    const auto& role=propertyOf(propertyOf(message,"message"),"role");
    if(role!="user")throw std::runtime_error(
      "Error: Expected message role 'user', got '"+textValue(role)+"'");
    output.resolve(std::move(message));
  }catch(const std::exception& error){
    settleFatal(line,error.what(),services,output);
  }catch(...){settleFatal(line,"unknown",services,output);}
  return output;
}
}
