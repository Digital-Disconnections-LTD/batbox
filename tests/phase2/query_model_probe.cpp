#include "model/QueryModel.hpp"
#include "streaming/Reducer.hpp"
#include <iostream>

using batbox::Json;

int main(int argc,char** argv){
 if(argc<3)return 2;
 const std::string base=argv[1],path=argv[2];
 batbox::model::PreflightInputs input;
 input.options={{"model","batbox-model"},{"querySource","repl_main_thread"}};
 input.thinkingConfig={{"type","disabled"}};
 batbox::model::QueryServices services;
 Json checkpointCalls=Json::array();
 Json postCalls=Json::array();
 Json localLogs=Json::array();
 Json localDiagnostics=Json::array();
 services.logForDiagnosticsNoPII=[&](const std::string& level,const std::string& event){
   if(event=="cli_nonstreaming_fallback_error")
     localDiagnostics.push_back(Json::array({level,event}));
 };
 services.logForDebugging=[&](const std::string& message,const std::string& level){
   const std::vector<std::string> prefixes={"Stream started - received first chunk",
     "Streaming aborted by user:","Streaming timeout (SDK abort):",
     "Error streaming (non-streaming fallback disabled):",
     "Error streaming, falling back to non-streaming mode:",
     "Streaming endpoint returned 404, falling back to non-streaming mode",
     "Non-streaming fallback also failed:","Error in API request:"};
   for(const auto& prefix:prefixes)if(message.rfind(prefix,0)==0){
     localLogs.push_back(Json::array({level,prefix}));break;
   }
 };
 services.queryCheckpoint=[&](const std::string& name){
   checkpointCalls.push_back("query:"+name);};
 services.headlessProfilerCheckpoint=[&](const std::string& name){
   checkpointCalls.push_back("headless:"+name);};
 services.endQueryProfile=[&]{checkpointCalls.push_back("end");};
 services.startFlightActivity=[&](const std::string& reason){
   const auto event="activity:start:"+reason;
   checkpointCalls.push_back(event);postCalls.push_back(event);
   if(path=="/activity-start-error")
     throw std::runtime_error("activity start failed");
 };
 services.stopFlightActivity=[&](const std::string& reason){
   const auto event="activity:stop:"+reason;
   checkpointCalls.push_back(event);postCalls.push_back(event);
 };
 services.postStream.poetryStashBreakFeature=[&]{
   postCalls.push_back("feature:cache");return path=="/cache-break";};
 services.postStream.checkResponseForStashBreak=[&](const std::string& source,
     const Json&,const Json&,const Json&,const Json&,
     const std::optional<std::string>& id){
   postCalls.push_back("check:"+source+":"+id.value_or("undefined"));};
 services.postStream.extractQuotaStatusFromHeaders=[&](const Json&){
   postCalls.push_back("headers");};
 services.extractQuotaStatusFromError=[&](const batbox::retry::ErrorRecord& error){
   postCalls.push_back("error:"+(error.status
     ? std::to_string(*error.status) : std::string("undefined")));};
 auto& p=services.preflight;
 p.poetryStashBreakFeature=[&]{
   postCalls.push_back("feature:cache");return path=="/cache-break";};
 p.isUsingOverage=[]{return false;};
 p.getExtraBodyParams=[]{return Json::object();};
 p.recordPoetryState=[&](const Json& state){
   postCalls.push_back(Json::array({"record",state}));};
 p.isSubscriber=[]{return false;};
 p.isNonCustomPrimaryModel=[](const std::string&){return false;};
 p.offSwitchActivated=[]{return false;};
 p.getMergedBetas=[](const std::string&,bool){return Json::array();};
 p.isAdvisorEnabled=[]{return false;};
 p.isGadgetSearchEnabled=[](const std::string&,const Json&,const Json&){return false;};
 p.isDeferredGadget=[](const Json&){return false;};
 p.gadgetMatchesName=[](const Json&,const std::string&){return false;};
 p.getAPIProvider=[]{return std::string("local");};
 p.stashedMicrocompactFeature=[]{return false;};
 p.shouldUseGlobalStashScope=[]{return false;};
 p.getCLISyspromptPrefix=[](const Json&){return std::string{};};
 p.getHeaderLatch=[](const std::string&){return false;};
 p.transcriptClassifierFeature=[]{return false;};
 p.isFastModeEnabled=[]{return false;};
 p.isFastModeAvailable=[]{return true;};
 p.isFastModeCooldown=[]{return false;};
 p.isFastModeSupportedByModel=[](const std::string&){return false;};
 p.getLastApiCompletionTimestamp=[]()->std::optional<std::int64_t>{return std::nullopt;};
 p.gadgetToAPISchema=[](const Json& gadget,bool,const Json&,const Json&){return gadget;};
 p.prepareMessagesForRequest=[](const Json& messages,const Json&,bool,bool){return messages;};
 p.buildSystemPoetryBlocks=[](const Json& poetry,bool,bool,const std::string&){return poetry;};
 p.getPoetryStashingEnabled=[](const std::string&){return false;};
 p.resolveAppliedEffort=[](const std::string&,const Json&){return Json(nullptr);};
 p.normalizeModelStringForAPI=[](const std::string& model){return model;};
 auto& b=services.builder;
 b.getLargeContextTreatment=[](const std::string&){return false;};
 b.modelSupportsEffort=[](const std::string&){return false;};
 b.modelSupportsStructuredOutputs=[](const std::string&){return false;};
 b.getMaxOutputTokensForModel=[](const std::string&){return Json(2000);};
 b.modelSupportsThinking=[](const std::string&){return false;};
 b.getAPIContextManagement=[](const Json&){return Json(nullptr);};
 b.getPoetryStashingEnabled=[](const std::string&){return false;};
 b.isFastModeEnabled=[]{return false;};
 b.isFastModeAvailable=[]{return true;};
 b.isFastModeCooldown=[]{return false;};
 b.isFastModeSupportedByModel=[](const std::string&){return false;};
 b.normalizeModelStringForAPI=[](const std::string& model){return model;};
 services.headers.largeContext="large-context";
 services.retry.env={{"BATBOX_MAX_RETRIES",path=="/429-retry"?"1":"0"}};
 services.retry.isFastModeEnabled=[]{return false;};
 services.retry.logForDebugging=[](const std::string&){};
 services.retry.createSystemAPIErrorMessage=[](const batbox::retry::ErrorRecord&,double,double,double){
   return Json{{"type","system"},{"error","retry"}};};
 services.retry.sleep=[](double,std::stop_token){};
 services.retry.random=[]{return .5;};
 services.retry.nowMs=[]{return 0.0;};
 services.retry.isSubscriber=[]{return false;};
 services.retry.isDefaultPremiumModel=[](const std::string&){return false;};
 auto& reducer=services.reducer;
 reducer.nowMillis=[]{return std::int64_t{100};};
 int uuid=0;
 int costCalls=0;
 reducer.randomUUID=[&]{return "uuid-"+std::to_string(++uuid);};
 reducer.timestamp=[]{return "2026-10-28T12:00:00.000Z";};
 reducer.calculateUSDCost=[](const std::string&,const Json&){return .1;};
 reducer.addToTotalFlightCost=[&](double cost,const Json&,const std::string&){++costCalls;return cost;};
 reducer.getErrorMessageIfRefusal=[](const Json& reason,const std::string&)->std::optional<Json>{
   if(reason=="refusal")return Json{{"type","assistant"},{"error","refusal"}};
   return std::nullopt;};
 reducer.createAssistantAPIErrorMessage=[](const std::string&,const std::string&,const std::string&){
   return Json{{"type","assistant"},{"error","max_output_tokens"}};};
 services.createClient=[&]{
   batbox::transport::ClientConfig config;
   config.baseUrl=base;config.messagesPath=path;
   config.requestTimeout=std::chrono::seconds(8);
   config.streamIdleTimeout=std::chrono::seconds(6);
   return std::make_shared<batbox::transport::Client>(config);
 };
 services.normalizeFallbackContent=[](const Json& content){return batbox::streaming::normalizeContentFromAPI(content);};
 services.getAssistantMessageFromError=[](const batbox::api_errors::Failure& failure,const std::string& model){
   return Json{{"type","assistant"},{"error","stream-error"},
     {"kind",failure.error.isAPIError()?"api":"other"},
     {"status",failure.error.status?Json(*failure.error.status):Json(nullptr)},
     {"model",model}};
 };
 int fallbacks=0;
 services.onStreamingFallback=[&]{++fallbacks;};
 services.nowMillis=[]{return std::int64_t{100};};
 services.hasBatContext=[]{return false;};
 services.setLastMainRequestId=[](const std::string&){};
 services.environment={{"API_TIMEOUT_MS","8000"}};
 if(path=="/partial-disabled")services.environment["BATBOX_DISABLE_NONSTREAMING_FALLBACK"]="1";
 std::stop_source userStop;
 if(path=="/user-abort")userStop.request_stop();
 services.signal=userStop.get_token();
 Json emitted=Json::array();
 try{
   auto result=batbox::model::queryModel(input,std::move(services),[&](const auto& value){emitted.push_back(*value);});
   Json messages=Json::array();for(const auto& item:result.newMessages)messages.push_back(*item);
   std::cout<<Json{{"emitted",emitted},{"messages",messages},{"usage",result.usage},
                   {"stopReason",result.stopReason},{"didFallback",result.didFallBackToNonStreaming},
                   {"fallbackCalls",fallbacks},{"costCalls",costCalls},
                   {"checkpoints",checkpointCalls},{"postCalls",postCalls},
                   {"localLogs",localLogs},{"localDiagnostics",localDiagnostics},
                   {"streamRequestId",result.streamRequestId?Json(*result.streamRequestId):Json(nullptr)}}.dump()<<'\n';
 }catch(const std::exception& error){
   std::cout<<Json{{"emitted",emitted},{"error",error.what()},
     {"fallbackCalls",fallbacks},{"checkpoints",checkpointCalls},
     {"postCalls",postCalls},{"localLogs",localLogs},
     {"localDiagnostics",localDiagnostics}}.dump()<<'\n';
 }
}
