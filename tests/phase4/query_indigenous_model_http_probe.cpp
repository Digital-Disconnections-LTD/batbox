#include "query/IndigenousModelCursor.hpp"
#include "query/OwnerModelCursor.hpp"
#include "streaming/Reducer.hpp"
#include <iostream>
#include <optional>

using batbox::Json;

int main(int argc,char** argv){
 if(argc<3)return 2;
 const std::string base=argv[1],path=argv[2];
 const bool delayedClientMode=argc>3&&std::string(argv[3])=="owner-delayed-client";
 const bool closeFallbackMode=argc>3&&std::string(argv[3])=="owner-close-fallback";
 const bool ownerMode=argc>3&&
   (std::string(argv[3])=="owner"||delayedClientMode||closeFallbackMode);
 batbox::event_loop::Loop loop;
 batbox::model::PreflightInputs input;
 input.options={{"model","batbox-model"},{"querySource","repl_main_thread"}};
 input.thinkingConfig={{"type","disabled"}};
 batbox::model::QueryServices services;
 Json checkpointCalls=Json::array();
 Json postCalls=Json::array();
 Json localLogs=Json::array(),localDiagnostics=Json::array();
 services.logForDebugging=[&](const std::string& message,const std::string& level){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("debug log off owner");
   localLogs.push_back(Json::array({level,message}));
 };
 services.logForDiagnosticsNoPII=[&](const std::string& level,const std::string& event){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("diagnostic log off owner");
   localDiagnostics.push_back(Json::array({level,event}));
 };
 services.queryCheckpoint=[&](const std::string& name){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("query checkpoint off owner");
   checkpointCalls.push_back("query:"+name);};
 services.headlessProfilerCheckpoint=[&](const std::string& name){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("headless checkpoint off owner");
   checkpointCalls.push_back("headless:"+name);};
 services.endQueryProfile=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("profile end off owner");
   checkpointCalls.push_back("end");};
 services.startFlightActivity=[&](const std::string& reason){
   if(ownerMode&&!loop.isOwnerThread())
     throw std::logic_error("activity start off owner");
   const auto event="activity:start:"+reason;
   checkpointCalls.push_back(event);postCalls.push_back(event);
   if(path=="/activity-start-error")
     throw std::runtime_error("activity start failed");
 };
 services.stopFlightActivity=[&](const std::string& reason){
   if(ownerMode&&!loop.isOwnerThread())
     throw std::logic_error("activity stop off owner");
   const auto event="activity:stop:"+reason;
   checkpointCalls.push_back(event);postCalls.push_back(event);
 };
 services.postStream.poetryStashBreakFeature=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("cache feature off owner");
   postCalls.push_back("feature:cache");return path=="/cache-break";};
 services.postStream.checkResponseForStashBreak=[&](const std::string& source,
     const Json&,const Json&,const Json&,const Json&,
     const std::optional<std::string>& id){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("cache check off owner");
   postCalls.push_back("check:"+source+":"+id.value_or("undefined"));};
 services.postStream.extractQuotaStatusFromHeaders=[&](const Json&){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("quota headers off owner");
   postCalls.push_back("headers");};
 services.extractQuotaStatusFromError=[&](const batbox::retry::ErrorRecord& error){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("quota error off owner");
   postCalls.push_back("error:"+(error.status
     ? std::to_string(*error.status) : std::string("undefined")));};
 auto& p=services.preflight;
 p.poetryStashBreakFeature=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("prompt cache feature off owner");
   postCalls.push_back("feature:cache");return path=="/cache-break";};
 p.isUsingOverage=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("overage state off owner");
   return false;};
 p.getExtraBodyParams=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("extra body off owner");
   return Json::object();};
 p.recordPoetryState=[&](const Json& state){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("prompt state off owner");
   postCalls.push_back(Json::array({"record",state}));};
 p.isSubscriber=[]{return false;};
 p.isNonCustomPrimaryModel=[](const std::string&){return false;};
 p.offSwitchActivated=[]{return false;};
 p.getMergedBetas=[](const std::string&,bool){return Json::array();};
 p.isAdvisorEnabled=[]{return false;};
 p.isGadgetSearchEnabled=[](const std::string&,const Json&,const Json&){return false;};
 p.isDeferredGadget=[](const Json&){return false;};
 p.gadgetMatchesName=[](const Json&,const std::string&){return false;};
 p.getAPIProvider=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("provider off owner");
   return std::string("local");};
 p.stashedMicrocompactFeature=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("completion feature off owner");
   return false;};
 p.shouldUseGlobalStashScope=[]{return false;};
 p.getCLISyspromptPrefix=[](const Json&){return std::string{};};
 p.getHeaderLatch=[](const std::string&){return false;};
 p.transcriptClassifierFeature=[]{return false;};
 p.isFastModeEnabled=[]{return false;};
 p.isFastModeAvailable=[]{return true;};
 p.isFastModeCooldown=[]{return false;};
 p.isFastModeSupportedByModel=[](const std::string&){return false;};
 p.getLastApiCompletionTimestamp=[]()->std::optional<std::int64_t>{return std::nullopt;};
 p.gadgetToAPISchema=[&](const Json& gadget,bool,const Json&,const Json&){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("schema off owner");
   return gadget;};
 p.prepareMessagesForRequest=[](const Json& messages,const Json&,bool,bool){return messages;};
 p.buildSystemPoetryBlocks=[](const Json& poetry,bool,bool,const std::string&){return poetry;};
 p.getPoetryStashingEnabled=[](const std::string&){return false;};
 p.resolveAppliedEffort=[](const std::string&,const Json&){return Json(nullptr);};
 p.normalizeModelStringForAPI=[&](const std::string& model){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("model normalize off owner");
   return model;};
 auto& b=services.builder;
 b.getLargeContextTreatment=[](const std::string&){return false;};
 b.modelSupportsEffort=[&](const std::string&){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("builder off owner");
   return false;};
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
 services.retry.nowMs=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("retry clock off owner");
   return 0.0;};
 services.retry.isSubscriber=[]{return false;};
 services.retry.isDefaultPremiumModel=[](const std::string&){return false;};
 auto& reducer=services.reducer;
 reducer.nowMillis=[]{return std::int64_t{100};};
 int uuid=0;
 int costCalls=0;
 reducer.randomUUID=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("message UUID off owner");
   return "uuid-"+std::to_string(++uuid);};
 reducer.timestamp=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("timestamp off owner");
   return "2026-10-28T12:00:00.000Z";};
 reducer.calculateUSDCost=[&](const std::string&,const Json&){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("cost calculation off owner");
   return .1;};
 reducer.addToTotalFlightCost=[&](double cost,const Json&,const std::string&){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("session cost off owner");
   ++costCalls;return cost;};
 reducer.getErrorMessageIfRefusal=[](const Json& reason,const std::string&)->std::optional<Json>{
   if(reason=="refusal")return Json{{"type","assistant"},{"error","refusal"}};
   return std::nullopt;};
 reducer.createAssistantAPIErrorMessage=[](const std::string&,const std::string&,const std::string&){
   return Json{{"type","assistant"},{"error","max_output_tokens"}};};
 services.createClient=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("client off owner");
   batbox::transport::ClientConfig config;
   config.baseUrl=base;config.messagesPath=path;
   config.requestTimeout=std::chrono::seconds(8);
   config.streamIdleTimeout=std::chrono::seconds(6);
   return std::make_shared<batbox::transport::Client>(config);
 };
 services.normalizeFallbackContent=[&](const Json& content){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("fallback content off owner");
   return batbox::streaming::normalizeContentFromAPI(content);};
 services.getAssistantMessageFromError=[&](const batbox::api_errors::Failure& failure,const std::string& model){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("error render off owner");
   return Json{{"type","assistant"},{"error","stream-error"},
     {"kind",failure.error.isAPIError()?"api":"other"},
     {"status",failure.error.status?Json(*failure.error.status):Json(nullptr)},
     {"model",model}};
 };
 int fallbacks=0;
 services.onStreamingFallback=[&]{++fallbacks;};
 services.nowMillis=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("attempt clock off owner");
   return std::int64_t{100};};
 services.hasBatContext=[&]{
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("agent context off owner");
   return false;};
 services.setLastMainRequestId=[&](const std::string&){
   if(ownerMode&&!loop.isOwnerThread())throw std::logic_error("request ID update off owner");};
 services.environment={{"API_TIMEOUT_MS","8000"}};
 if(path=="/watchdog"){
   services.environment["BATBOX_ENABLE_STREAM_WATCHDOG"]="1";
   services.environment["BATBOX_STREAM_IDLE_TIMEOUT_MS"]="40";
 }
 if(path=="/partial-disabled")services.environment["BATBOX_DISABLE_NONSTREAMING_FALLBACK"]="1";
 std::stop_source userStop;
 if(path=="/user-abort")userStop.request_stop();
 services.signal=userStop.get_token();
 Json emitted=Json::array();
 int ownerHeartbeats=0;
 bool closedFallback=false;
 try{
   int cursorFallbacks=0;
   batbox::query::OwnerPreflightAwaits awaits;
   const auto search=p.isGadgetSearchEnabled;
   const auto schema=p.gadgetToAPISchema;
   awaits.gadgetSearchEnabled=[&loop,search](const std::string& model,
       const Json& gadgets,const Json& options){
     batbox::event_loop::Deferred<bool> result(loop);
     result.resolve(search(model,gadgets,options));return result;};
   awaits.gadgetToAPISchema=[&loop,schema](const Json& gadget,bool defer,
       const Json& allGadgets,const Json& options){
     batbox::event_loop::Deferred<Json> result(loop);
     result.resolve(schema(gadget,defer,allGadgets,options));return result;};
   batbox::query::OwnerRequestAwaits requestAwaits;
   if(delayedClientMode){
     const auto createClient=services.createClient;
     requestAwaits.createClient=[&loop,&ownerHeartbeats,createClient]{
       batbox::event_loop::Deferred<std::shared_ptr<batbox::transport::Client>>
         pending(loop);
       loop.setTimeout(5,[&ownerHeartbeats]{++ownerHeartbeats;},true);
       loop.setTimeout(20,[pending,createClient]()mutable{
         try{pending.resolve(createClient());}
         catch(...){pending.reject(std::current_exception());}
       },true);
       return pending;
     };
   }
   auto cursor=ownerMode?
     batbox::query::makeOwnerSegmentQueryModelCursor(loop,input,
       std::move(services),awaits,[&]{++cursorFallbacks;},requestAwaits):
     batbox::query::makeIndigenousQueryModelCursor(loop,input,
       std::move(services),[&]{++cursorFallbacks;});
   while(true){
     auto pending=cursor.next();
     std::optional<std::optional<batbox::model::MessageRef>> value;
     std::exception_ptr error;
     pending.then([&](const std::optional<batbox::model::MessageRef>* item,
         const std::exception_ptr& rejected){
       error=rejected;if(item)value=*item;loop.stop();});
     auto hold=loop.hold();loop.run();hold.release();
     if(error)std::rethrow_exception(error);
     if(!value)throw std::logic_error("Native model cursor did not settle");
     if(!*value)break;
     emitted.push_back(***value);
     if(closeFallbackMode && batbox::value::property(***value,"type")=="assistant"){
       if(!cursor.close)throw std::logic_error("Owner model close is missing");
       auto closing=cursor.close();
       bool closed=false;std::exception_ptr closeError;
       closing.then([&](const batbox::event_loop::Unit* done,
           const std::exception_ptr& rejected){
         closed=done!=nullptr;closeError=rejected;loop.stop();});
       auto keep=loop.hold();loop.run();keep.release();
       if(closeError)std::rethrow_exception(closeError);
       if(!closed)throw std::logic_error("Fallback close did not settle");
       closedFallback=true;
       break;
     }
   }
   std::cout<<Json{{"emitted",emitted},{"fallbackCalls",fallbacks},
     {"cursorFallbackCalls",cursorFallbacks},{"costCalls",costCalls},
     {"ownerHeartbeats",ownerHeartbeats},{"closedFallback",closedFallback},
     {"checkpoints",checkpointCalls},{"postCalls",postCalls},
     {"localLogs",localLogs},{"localDiagnostics",localDiagnostics}}.dump()<<'\n';
 }catch(const std::exception& error){
   std::cout<<Json{{"emitted",emitted},{"error",error.what()},
     {"fallbackCalls",fallbacks},{"checkpoints",checkpointCalls},
     {"postCalls",postCalls},{"localLogs",localLogs},
     {"localDiagnostics",localDiagnostics}}.dump()<<'\n';
 }
}
