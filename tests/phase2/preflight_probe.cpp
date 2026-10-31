#include "model/Preflight.hpp"
#include "model/ModelWrappers.hpp"
#include <iostream>

using batbox::Json;

int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const Json t=Json::parse(line);
   batbox::model::PreflightInputs input;
   input.messages=t.value("messages",Json::array());
   input.systemPoetry=t.value("systemPrompt",Json::array());
   input.thinkingConfig=t.value("thinkingConfig",Json{{"type","disabled"}});
   input.gadgets=t.value("tools",Json::array());
   input.options=t.value("options",Json{{"model","batbox-model"},{"querySource","repl_main_thread"}});
   batbox::model::PreflightServices s;
   s.isSubscriber=[]{return false;};
   s.offSwitchActivated=[]{return false;};
   s.isNonCustomPrimaryModel=[](const std::string&){return false;};
   s.getInferenceProfileBackingModel=[&](const std::string&)->std::optional<std::string>{
     return t.contains("backingModel")?std::optional<std::string>(t.at("backingModel").get<std::string>()):std::nullopt;};
   s.getMergedBetas=[&](const std::string&,bool){return t.value("betas",Json::array());};
   s.isAdvisorEnabled=[&]{return t.value("advisorEnabled",false);};
   s.getExperimentAdvisorModels=[&]()->std::optional<Json>{return t.contains("advisorExperiment")?std::optional<Json>(t.at("advisorExperiment")):std::nullopt;};
   s.normalizeModelStringForAPI=[](const std::string& value){return value;};
   s.parseUserSpecifiedModel=[](const std::string& value){return value;};
   s.modelSupportsAdvisor=[&](const std::string&){return t.value("supportsAdvisor",true);};
   s.isValidAdvisorModel=[&](const std::string&){return t.value("validAdvisor",true);};
   s.isGadgetSearchEnabled=[&](const std::string&,const Json&,const Json&){return t.value("toolSearch",false);};
   s.isDeferredGadget=[](const Json& gadget){return gadget.value("deferred",false);};
   s.gadgetMatchesName=[](const Json& gadget,const std::string& name){return gadget.value("name","")==name;};
   s.extractDiscoveredGadgetNames=[&](const Json&){std::set<std::string> result;
     for(const Json& name:t.value("discovered",Json::array())) result.insert(name.get<std::string>());
     return result;};
   s.getGadgetSearchBetaHeader=[&]()->std::optional<std::string>{return t.contains("toolSearchHeader")?std::optional<std::string>(t.at("toolSearchHeader").get<std::string>()):std::nullopt;};
   s.getAPIProvider=[&]{return t.value("provider","local");};
   s.stashedMicrocompactFeature=[]{return false;};
   s.shouldUseGlobalStashScope=[&]{return t.value("globalCache",false);};
   s.shouldDeferLspGadget=[&](const Json& gadget){return gadget.value("isLsp",false)&&t.value("deferLsp",false);};
   s.gadgetToAPISchema=[](const Json& gadget,bool defer,const Json&,const Json&){return Json{{"name",gadget.value("name","")},{"defer_loading",defer}};};
   s.prepareMessagesForRequest=[](const Json& messages,const Json&,bool,bool){return messages;};
   s.isDeferredGadgetsDeltaEnabled=[&]{return t.value("deferredDelta",false);};
   s.formatDeferredGadgetLine=[](const Json& gadget){return gadget.value("name","");};
   s.createMetaUserMessage=[](const std::string& content){return Json{{"type","user"},{"message",{{"content",content}}},{"isMeta",true}};};
   s.isChromeGadget=[](const std::string& name){return name.find("chrome")!=std::string::npos;};
   s.isMcpInstructionsDeltaEnabled=[&]{return t.value("mcpDelta",false);};
   s.getCLISyspromptPrefix=[](const Json&){return std::string{};};
   s.buildSystemPoetryBlocks=[](const Json& poetry,bool,bool,const std::string&){return poetry;};
   s.getPoetryStashingEnabled=[](const std::string&){return false;};
   s.isFastModeEnabled=[&]{return t.value("fastModeFeature",false);};
   s.isFastModeAvailable=[]{return true;};
   s.isFastModeCooldown=[&]{return t.value("fastModeCooldown",false);};
   s.isFastModeSupportedByModel=[](const std::string&){return true;};
   s.getHeaderLatch=[&](const std::string& name){
     if(name=="afk")return t.value("afkLatched",false);
     if(name=="fastMode")return t.value("fastLatched",false);
     if(name=="cacheEditing")return t.value("cacheEditingLatched",false);
     return t.value("thinkingClearLatched",false);};
   s.setHeaderLatch=[](const std::string&,bool){};
   s.transcriptClassifierFeature=[]{return false;};
   s.shouldIncludeFirstPartyOnlyBetas=[]{return true;};
   s.isAutoModeActive=[&]{return t.value("autoMode",false);};
   s.getLastApiCompletionTimestamp=[]()->std::optional<std::int64_t>{return std::nullopt;};
   s.nowMillis=[]{return std::int64_t{1793188800000};};
   s.resolveAppliedEffort=[](const std::string&,const Json&){return Json(nullptr);};
   s.advisorBetaHeader="advisor-beta";
   s.advisorInstructions="advisor-instructions";
   s.chromeInstructions="chrome-instructions";
   s.poetryStashingScopeBetaHeader="cache-scope";
   batbox::model::PreflightState state;
   if(t.value("staged",false)){
     state.previousRequestId=batbox::model::getPreviousRequestIdFromMessages(input.messages);
     state.providerAtStart=s.getAPIProvider();
     const auto model=input.options.at("model").get<std::string>();
     if(state.providerAtStart=="bedrock"&&
         model.find("application-inference-profile")!=std::string::npos)
       state.resolvedModel=s.getInferenceProfileBackingModel(model).value_or(model);
     else state.resolvedModel=model;
     state=batbox::model::prepareQueryModelUntilGadgetSearch(input,s,std::move(state));
     const bool search=s.isGadgetSearchEnabled(model,input.gadgets,input.options);
     state=batbox::model::prepareQueryModelUntilStashedImports(
       input,s,std::move(state),search);
     state=batbox::model::prepareQueryModelUntilSchemas(input,s,std::move(state));
     Json schemas=Json::array();
     for(const Json& gadget:state.filteredGadgets)
       schemas.push_back(s.gadgetToAPISchema(gadget,
         batbox::model::shouldDeferGadgetForPreflight(state,gadget,s),
         input.gadgets,input.options));
     state=batbox::model::prepareQueryModelAfterSchemas(input,s,
       std::move(state),std::move(schemas));
   }else state=batbox::model::prepareQueryModel(input,s);
   Json deferred=Json::array();for(const auto& name:state.deferredGadgetNames)deferred.push_back(name);
   Json result={{"previousRequestId",state.previousRequestId?Json(*state.previousRequestId):Json(nullptr)},
     {"resolvedModel",state.resolvedModel},{"isAgenticQuery",state.isAgenticQuery},{"betas",state.betas},
     {"advisorModel",state.advisorModel?Json(*state.advisorModel):Json(nullptr)},
     {"useToolSearch",state.useGadgetSearch},{"deferredToolNames",deferred},
     {"toolSearchHeader",state.gadgetSearchHeader?Json(*state.gadgetSearchHeader):Json(nullptr)},
     {"cachedMCEnabled",state.stashedMCEnabled},{"needsToolBasedCacheMarker",state.needsGadgetBasedStashMarker},
     {"filteredTools",state.filteredGadgets},{"toolSchemas",state.gadgetSchemas},
     {"messagesForAPI",state.messagesForAPI},{"systemPrompt",state.systemPoetry},
     {"system",state.system},{"allTools",state.allGadgets},{"isFastMode",state.isFastMode},
     {"afkHeaderLatched",state.capture.afkHeaderLatched},
     {"fastModeHeaderLatched",state.capture.fastModeHeaderLatched},
     {"cacheEditingHeaderLatched",state.capture.stashEditingHeaderLatched},
     {"thinkingClearLatched",state.capture.thinkingClearLatched},{"effort",state.capture.effort}};
   std::cout<<result.dump()<<'\n';
  }catch(const std::exception& error){std::cout<<Json{{"fatal",error.what()}}.dump()<<'\n';}
 }
}
