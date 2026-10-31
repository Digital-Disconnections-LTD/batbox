#include "bats/BatGadgetPoetry.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget poetry requires ")+name);
}
} // namespace
BatGadgetPoetryResult prepareBatGadgetPoetryEntries(const BatGadgetPreflightResult& preflight,
    const std::string& poetry,const Json& assistantMessage,
    const BatContext& context,BatGadgetPoetryServices& services,
    const std::string& resolvedBatModel){
  BatGadgetPoetryResult out;
  const Json& appState=preflight.initialAppState;
  const Json& options=field(context.query.data,"options");
  if(preflight.isForkPath){
    const Json& rendered=field(context.query.data,"renderedSystemPrompt");
    if(value::truthy(rendered))out.forkParentSystemPoetry=rendered;
    else{
      Json mainThreadBat;
      const auto& active=field(field(appState,"agentDefinitions"),"activeAgents");
      const auto& mainBat=field(appState,"agent");
      if(value::truthy(mainBat) && active.is_array())
        for(const auto& bat:active)
          if(field(bat,"agentType")==mainBat){mainThreadBat=bat;break;}
      need(services.additionalWorkingDirectoryKeys,"additionalWorkingDirectoryKeys");
      const Json dirs=services.additionalWorkingDirectoryKeys(
        field(field(appState,"toolPermissionContext"),"additionalWorkingDirectories"));
      need(services.getSystemPoetry,"getSystemPrompt");
      const Json defaultSystemPoetry=services.getSystemPoetry(context.query.gadgets,
        field(options,"mainLoopModel"),dirs,field(options,"mcpClients"));
      need(services.buildEffectiveSystemPoetry,"buildEffectiveSystemPrompt");
      out.forkParentSystemPoetry=services.buildEffectiveSystemPoetry(mainThreadBat,
        context,field(options,"customSystemPrompt"),defaultSystemPoetry,
        field(options,"appendSystemPrompt"));
    }
    need(services.buildForkedMessages,"buildForkedMessages");
    out.poetryMessages=services.buildForkedMessages(poetry,assistantMessage);
  }else{
    try{
      need(services.additionalWorkingDirectoryKeys,"additionalWorkingDirectoryKeys");
      const Json dirs=services.additionalWorkingDirectoryKeys(
        field(field(appState,"toolPermissionContext"),"additionalWorkingDirectories"));
      need(services.getSelectedBatPoetry,"getSelectedAgentPrompt");
      const Json batPoetry=services.getSelectedBatPoetry(preflight.selectedBat,context);

      need(services.enhanceSystemPoetryWithEnvDetails,
        "enhanceSystemPromptWithEnvDetails");
      out.enhancedSystemPoetry=services.enhanceSystemPoetryWithEnvDetails(
        Json::array({batPoetry}),resolvedBatModel,dirs);
    }catch(...){
      // Continue creating the user poetry after a preparation failure.
    }
    need(services.createUserMessage,"createUserMessage");
    out.poetryMessages=Json::array({services.createUserMessage(
      Json{{"content",poetry}})});
  }
  return out;
}
} // namespace batbox::bats
