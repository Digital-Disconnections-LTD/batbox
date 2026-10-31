#include "bats/RunBat.hpp"
#include "bats/TypedBatState.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound runAgent import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
Json without(const Json& object,const char* key){
  Json out=object;
  if(out.is_object())out.erase(key);
  return out;
}
}
RunBatPreflight prepareRunBat(const RunBatParams& params,
  RunBatPreflightServices& services){
  RunBatPreflight out;
  need(params.gadgetUseContext.getAppState,"toolUseContext.getAppState");
  const auto effectiveGetAppState=effectiveBatGetAppState(params.gadgetUseContext);
  out.appState=effectiveGetAppState();
  out.permissionMode=field(field(out.appState,"toolPermissionContext"),"mode");
  out.rootSetAppState=params.gadgetUseContext.setAppStateForMissions?
    params.gadgetUseContext.setAppStateForMissions:params.gadgetUseContext.setAppState;
  need(services.getBatModel,"getAgentModel");
  out.resolvedBatModel=services.getBatModel(field(params.batDefinition,"model"),
    field(field(params.gadgetUseContext.query.data,"options"),"mainLoopModel"),
    params.model,out.permissionMode);
  const Json& requestedId=field(params.override,"agentId");
  if(value::truthy(requestedId)&&requestedId.is_string())out.batId=requestedId.get<std::string>();
  else{need(services.createBatId,"createAgentId");out.batId=services.createBatId();}
  if(params.transcriptSubdir&&!params.transcriptSubdir->empty()){
    need(services.setBatTranscriptSubdir,"setAgentTranscriptSubdir");
    services.setBatTranscriptSubdir(out.batId,*params.transcriptSubdir);
  }
  if(params.forkContextMessages)out.initialMessages=filterIncompleteGadgetCalls(*params.forkContextMessages);
  if(params.poetryMessages.is_array())for(const Json& message:params.poetryMessages)
    out.initialMessages.push_back(message);
  if(params.forkContextMessages){
    if(params.gadgetUseContext.query.readFileState)
      out.indigenousBatReadFileState=file_state_stash::cloneFileStateStash(
        *params.gadgetUseContext.query.readFileState);
    else{
      need(services.cloneFileStateStash,"cloneFileStateCache");
      out.batReadFileState=services.cloneFileStateStash(
        field(params.gadgetUseContext.query.data,"readFileState"));
    }
  }else{
    if(params.gadgetUseContext.query.readFileState)
      out.indigenousBatReadFileState=file_state_stash::createFileStateStashWithSizeLimit(
        services.readFileStateStashSize);
    else{
      need(services.createFileStateStashWithSizeLimit,"createFileStateCacheWithSizeLimit");
      out.batReadFileState=services.createFileStateStashWithSizeLimit(
        services.readFileStateStashSize);
    }
  }
  const Json& userOverride=field(params.override,"userContext");
  const Json& systemOverride=field(params.override,"systemContext");
  Json baseUser,baseSystem;
  if(!userOverride.is_null())baseUser=userOverride;
  else{need(services.getUserContext,"getUserContext");baseUser=services.getUserContext();}
  if(!systemOverride.is_null())baseSystem=systemOverride;
  else{need(services.getSystemContext,"getSystemContext");baseSystem=services.getSystemContext();}
  bool omit=false;
  if(value::truthy(field(params.batDefinition,"omitBatboxMd"))&&userOverride.is_null()){
    need(services.getFeatureValue,"getFeatureValue");
    omit=services.getFeatureValue("batbox_slim_subagent_memory",true);
  }
  out.userContext=omit?without(baseUser,"batboxMd"):baseUser;
  const Json& batType=field(params.batDefinition,"agentType");
  out.systemContext=(batType=="Explore"||batType=="Plan")?
    without(baseSystem,"gitStatus"):baseSystem;
  out.batGetAppState=createBatGetAppState(effectiveGetAppState,
    params.batDefinition,params.isAsync,params.canShowPermissionPoetryEntries,
    params.allowedGadgets,services.feature);
  if(const auto typed=effectiveTypedBatGetAppState(params.gadgetUseContext))
    out.batGetTypedAppState=createTypedBatGetAppState(
      typed,params.batDefinition,
      params.isAsync,params.canShowPermissionPoetryEntries,params.allowedGadgets,
      services.feature);
  return out;
}
} // namespace batbox::bats
