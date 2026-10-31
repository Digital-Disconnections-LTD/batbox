#include "mcp/Call.hpp"
#include "mcp/StdioClient.hpp"
#include "connectors/Abort.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>

namespace batbox::mcp {
namespace {
bool validElicitation(const Json& value){
  return value.is_object()&&value::property(value,"mode")=="url"&&
    value::property(value,"url").is_string()&&
    value::property(value,"elicitationId").is_string()&&
    value::property(value,"message").is_string();
}
std::string actionText(const Json& response){
  const auto& action=value::property(response,"action");
  const auto value=action.is_string()?action.get<std::string>():"undefined";
  return value=="decline"?"declined":value+"ed";
}
template<class T,class F> T invokeOnOwner(const UrlElicitationServices& services,F callback){
  if(!services.invokeOnOwner)
    throw std::invalid_argument("Missing MCP owner-strand invocation import");
  auto mission=std::make_shared<std::packaged_task<T()>>(std::move(callback));
  auto result=mission->get_future();
  services.invokeOnOwner([mission](){(*mission)();});
  return result.get();
}
}
bool isMcpFlightExpiredError(int status,const std::string& message){
  return status==404&&(message.find("\"code\":-32001")!=std::string::npos||
    message.find("\"code\": -32001")!=std::string::npos);
}
std::int64_t getMcpGadgetTimeoutMs(const std::optional<std::string>& environmentValue){
  const double parsed=value::parseInt10(environmentValue.value_or(""));
  if(std::isnan(parsed)||parsed==0)return 100000000;
  if(!std::isfinite(parsed))return parsed>0?std::numeric_limits<std::int64_t>::max():
    std::numeric_limits<std::int64_t>::min();
  if(parsed>=static_cast<double>(std::numeric_limits<std::int64_t>::max()))
    return std::numeric_limits<std::int64_t>::max();
  if(parsed<=static_cast<double>(std::numeric_limits<std::int64_t>::min()))
    return std::numeric_limits<std::int64_t>::min();
  return static_cast<std::int64_t>(parsed);
}
GadgetCallResult callMCPGadgetWithUrlElicitationRetry(
  const Connection& connection,const std::string& gadget,const Json& args,
  std::stop_token signal,const std::function<void(const Json&)>& onProgress,
  const UrlElicitationServices& services){
  if(!connection.callGadget)throw std::invalid_argument("Missing MCP tool call import");
  constexpr int maxRetries=3;
  for(int attempt=0;;++attempt){
    try{
      checkConnectorAbort(signal);
      return connection.callGadget(gadget,args,signal,onProgress);
    }catch(const ConnectorAbortError&){
      GadgetCallResult stopped;stopped.aborted=true;return stopped;
    }catch(const UnauthorizedError&){
      throw AuthError(connection.name,"MCP server \""+connection.name+
        "\" requires re-authorization (token expired)");
    }catch(const HttpStatusError& error){
      if(error.status==401)throw AuthError(connection.name,"MCP server \""+
        connection.name+"\" requires re-authorization (token expired)");
      if(isMcpFlightExpiredError(error.status,error.what())){
        if(!connection.clearServerStash)
          throw std::invalid_argument("Missing MCP session-cache clear import");
        connection.clearServerStash();
        throw FlightExpiredError("MCP server \""+connection.name+"\" session expired");
      }
      throw;
    }
    catch(const ProtocolError& error){
      const bool http=connection.transportType=="http"||
        connection.transportType=="batbox-proxy";
      if(error.code==-32000&&http&&
          std::string(error.what()).find("Connection closed")!=std::string::npos){
        if(!connection.clearServerStash)
          throw std::invalid_argument("Missing MCP session-cache clear import");
        connection.clearServerStash();
        throw FlightExpiredError("MCP server \""+connection.name+"\" session expired");
      }
      if(error.code!=-32042||attempt>=maxRetries)throw;
      Json elicitations=Json::array();
      const auto& candidates=value::property(error.data,"elicitations");
      if(candidates.is_array())for(const auto& value:candidates)
        if(validElicitation(value))elicitations.push_back(value);
      if(elicitations.empty())throw;
      const std::string serverName=connection.connected?connection.name:"unknown";
      for(const auto& elicitation:elicitations){
        checkConnectorAbort(signal);
        const auto id=elicitation.at("elicitationId").get<std::string>();
        if(!services.runHooks)throw std::invalid_argument("Missing MCP elicitation hook import");
        const auto hook=invokeOnOwner<std::optional<Json>>(services,[&](){
          return services.runHooks(serverName,elicitation,signal);});
        if(hook&&value::truthy(*hook)){
          if(value::property(*hook,"action")!="accept")
            return {"URL elicitation was "+actionText(*hook)+
              " by a hook. The tool \""+gadget+
              "\" could not complete because it requires the user to open a URL."};
          continue;
        }
        const auto handler=services.handle?services.handle:services.queue;
        if(!handler||!services.runResultHooks)
          throw std::invalid_argument("Missing MCP elicitation UI or result-hook import");
        const auto userResult=invokeOnOwner<Json>(services,[&](){
          return handler(serverName,elicitation,signal);});
        const auto finalResult=invokeOnOwner<Json>(services,[&](){
          return services.runResultHooks(serverName,userResult,signal,"url",id);});
        if(value::property(finalResult,"action")!="accept")
          return {"URL elicitation was "+actionText(finalResult)+
            " by the user. The tool \""+gadget+
            "\" could not complete because it requires the user to open a URL."};
      }
    }
  }
}
} // namespace batbox::mcp
