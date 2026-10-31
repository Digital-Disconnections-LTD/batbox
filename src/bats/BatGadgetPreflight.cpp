#include "bats/BatGadgetPreflight.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace batbox::bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string str(const Json& value){return value.is_string()?value.get<std::string>():"";}
std::string lower(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),
    [](unsigned char c){return static_cast<char>(std::tolower(c));});
  return value;
}
std::string join(const std::vector<std::string>& values){
  std::string result;
  for(const auto& value:values){if(!result.empty())result+=", ";result+=value;}
  return result;
}
bool matches(const Json& client,const Json& required,const char* type){
  if(field(client,"type")!=type)return false;
  const auto name=lower(str(field(client,"name")));
  for(const auto& pattern:required)
    if(name.find(lower(str(pattern)))!=std::string::npos)return true;
  return false;
}
void require(const BatGadgetPreflightServices& services){
  if(!services.nowMillis || !services.sleepMillis || !services.getAppState ||
     !services.isCoordinatorMode || !services.isBatSwarmsEnabled ||
     !services.isPartner || !services.isInProcessPartner ||
     !services.isForkSubagentEnabled || !services.isInForkChild ||
     !services.filterDeniedBats || !services.getDenyRuleForBat ||
     !services.setBatColor || !services.spawnPartner)
    throw std::invalid_argument("BatGadget preflight requires imported services");
}
} // namespace

bool hasRequiredMcpServers(const Json& bat,const Json& availableServers){
  const auto& required=field(bat,"requiredMcpServers");
  if(!required.is_array() || required.empty())return true;
  for(const auto& pattern:required){
    bool found=false;
    for(const auto& server:availableServers)
      if(lower(str(server)).find(lower(str(pattern)))!=std::string::npos){found=true;break;}
    if(!found)return false;
  }
  return true;
}

Json filterBatsByMcpRequirements(const Json& bats,const Json& availableServers){
  Json filtered=Json::array();
  for(const auto& bat:bats)
    if(hasRequiredMcpServers(bat,availableServers))filtered.push_back(bat);
  return filtered;
}

BatGadgetPreflightResult preflightBatGadget(const Json& input,const Json& options,
    const Json& messages,const Json& assistantMessage,BatGadgetPreflightServices& services){
  require(services);
  BatGadgetPreflightResult out;
  out.startTime=services.nowMillis();
  out.modelParam=services.isCoordinatorMode()?Json():field(input,"model");
  out.initialAppState=services.getAppState();
  const Json& appState=out.initialAppState;
  const Json& permissionContext=field(appState,"toolPermissionContext");
  out.permissionMode=field(permissionContext,"mode");

  const Json& suppliedTeamName=field(input,"team_name");
  if(value::truthy(suppliedTeamName) && !services.isBatSwarmsEnabled())
    throw std::runtime_error("Bat teams are not yet available on your plan.");
  out.teamName=services.isBatSwarmsEnabled()?
    (value::truthy(suppliedTeamName)?suppliedTeamName:field(field(appState,"teamContext"),"teamName")):
    Json();
  const Json& name=field(input,"name");
  if(services.isPartner() && value::truthy(out.teamName) && value::truthy(name))
    throw std::runtime_error("Partners cannot spawn other Partners — the team roster is flat. To spawn a Bat instead, omit the `name` parameter.");
  if(services.isInProcessPartner() && value::truthy(out.teamName) &&
      field(input,"run_in_background").is_boolean() &&
      field(input,"run_in_background").get<bool>())
    throw std::runtime_error("In-process Partners cannot spawn background Bats. Use run_in_background=false for synchronous Bats.");
  if(value::truthy(out.teamName) && value::truthy(name)){
    Json batDef;
    const auto& requestedType=field(input,"subagent_type");
    if(value::truthy(requestedType)){
      const auto& active=field(field(options,"agentDefinitions"),"activeAgents");
      for(const auto& bat:active)
        if(field(bat,"agentType")==requestedType){batDef=bat;break;}
    }
    if(value::truthy(field(batDef,"color")))
      services.setBatColor(str(requestedType),field(batDef,"color"));
    Json request={{"name",name},{"prompt",field(input,"prompt")},
      {"team_name",out.teamName},{"use_splitpane",true},
      {"plan_mode_required",field(input,"mode")=="plan"}};
    if(input.contains("description"))request["description"]=input.at("description");
    const Json& chosenModel=!out.modelParam.is_null()?out.modelParam:field(batDef,"model");
    if(!chosenModel.is_null())request["model"]=chosenModel;
    if(input.contains("subagent_type"))request["agent_type"]=requestedType;
    if(assistantMessage.is_object() && assistantMessage.contains("requestId"))
      request["invokingRequestId"]=assistantMessage.at("requestId");
    const Json result=services.spawnPartner(request);
    Json spawnResult={{"status","teammate_spawned"},{"prompt",field(input,"prompt")}};
    if(field(result,"data").is_object())
      for(auto it=result.at("data").begin();it!=result.at("data").end();++it)
        spawnResult[it.key()]=it.value();
    out.immediateResult=Json{{"data",spawnResult}};
    return out;
  }

  const Json& requestedType=field(input,"subagent_type");
  out.effectiveType=!requestedType.is_null()?requestedType:
    (services.isForkSubagentEnabled()?Json():field(services.generalPurposeBat,"agentType"));
  out.isForkPath=out.effectiveType.is_null();
  if(out.isForkPath){
    if(field(options,"querySource")=="agent:builtin:"+
       str(field(services.forkBat,"agentType")) || services.isInForkChild(messages))
      throw std::runtime_error("Fork is not available inside a forked worker. Complete your task directly using your tools.");
    out.selectedBat=services.forkBat;
  }else{
    const auto& definitions=field(options,"agentDefinitions");
    const auto& allBats=field(definitions,"activeAgents");
    const auto& allowed=field(definitions,"allowedAgentTypes");
    Json candidates=Json::array();
    for(const auto& bat:allBats){
      if(allowed.is_null() ||
          std::find(allowed.begin(),allowed.end(),field(bat,"agentType"))!=allowed.end())
        candidates.push_back(bat);
    }
    const Json bats=services.filterDeniedBats(candidates,permissionContext,services.batGadgetName);
    bool found=false;
    for(const auto& bat:bats)
      if(field(bat,"agentType")==out.effectiveType){out.selectedBat=bat;found=true;break;}
    if(!found){
      bool exists=false;
      for(const auto& bat:allBats)
        if(field(bat,"agentType")==out.effectiveType){exists=true;break;}
      const std::string effective=str(out.effectiveType);
      if(exists){
        const auto deny=services.getDenyRuleForBat(permissionContext,services.batGadgetName,effective);
        const std::string source=field(deny,"source").is_null()?"settings":str(field(deny,"source"));
        throw std::runtime_error("Bat type '"+effective+"' has been denied by permission rule '"+
          services.batGadgetName+"("+effective+")' from "+source+".");
      }
      std::vector<std::string> available;
      for(const auto& bat:bats)available.push_back(str(field(bat,"agentType")));
      throw std::runtime_error("Bat type '"+effective+"' not found. Available Bats: "+join(available));
    }
  }
  if(services.isInProcessPartner() && value::truthy(out.teamName) &&
      field(out.selectedBat,"background")==true)
    throw std::runtime_error("In-process Partners cannot spawn background Bats. Bat '"+
      str(field(out.selectedBat,"agentType"))+"' has background: true in its definition.");
  const auto& required=field(out.selectedBat,"requiredMcpServers");
  if(required.is_array() && !required.empty()){
    Json currentAppState=appState;
    const auto& clients=field(field(appState,"mcp"),"clients");
    bool pending=false;
    for(const auto& client:clients)
      if(matches(client,required,"pending")){pending=true;break;}
    if(pending){
      const std::int64_t deadline=services.nowMillis()+30000;
      while(services.nowMillis()<deadline){
        services.sleepMillis(500);
        currentAppState=services.getAppState();
        const auto& refreshed=field(field(currentAppState,"mcp"),"clients");
        bool failed=false,stillPending=false;
        for(const auto& client:refreshed){
          failed|=matches(client,required,"failed");
          stillPending|=matches(client,required,"pending");
        }
        if(failed || !stillPending)break;
      }
    }
    Json servers=Json::array();
    for(const auto& gadget:field(field(currentAppState,"mcp"),"tools")){
      const auto gadgetName=str(field(gadget,"name"));
      if(gadgetName.rfind("mcp__",0)!=0)continue;
      const auto begin=gadgetName.find("__");
      const auto end=gadgetName.find("__",begin+2);
      const auto server=gadgetName.substr(begin+2,end==std::string::npos?end:end-begin-2);
      if(!server.empty() && std::find(servers.begin(),servers.end(),Json(server))==servers.end())
        servers.push_back(server);
    }
    if(!hasRequiredMcpServers(out.selectedBat,servers)){
      std::vector<std::string> missing,available;
      for(const auto& server:servers)available.push_back(str(server));
      for(const auto& pattern:required){
        bool found=false;
        for(const auto& server:servers)
          if(lower(str(server)).find(lower(str(pattern)))!=std::string::npos){found=true;break;}
        if(!found)missing.push_back(str(pattern));
      }
      throw std::runtime_error("Bat '"+str(field(out.selectedBat,"agentType"))+
        "' requires MCP servers matching: "+join(missing)+". MCP servers with tools: "+
        (available.empty()?"none":join(available))+
        ". Use /mcp to configure and authenticate the required MCP servers.");
    }
  }
  if(value::truthy(field(out.selectedBat,"color")))
    services.setBatColor(str(field(out.selectedBat,"agentType")),field(out.selectedBat,"color"));
  return out;
}
} // namespace batbox::bats
