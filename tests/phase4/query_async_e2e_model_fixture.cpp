#include "query_async_e2e_model_fixture.hpp"
#include <stdexcept>

using batbox::Json;
batbox::model::QueryServices QueryE2EModelFixture::makeServices(){
  batbox::model::QueryServices services;
  services.logForDebugging=[this](const std::string& message,const std::string& level){
    if(owner&&!owner->isOwnerThread())throw std::logic_error("Debug logger ran off owner");
    localLogs.emplace_back(level,message);
  };
  services.logForDiagnosticsNoPII=[this](const std::string& level,
      const std::string& event){
    if(owner&&!owner->isOwnerThread())throw std::logic_error("Diagnostic logger ran off owner");
    localDiagnostics.emplace_back(level,event);
  };
  services.postStream.poetryStashBreakFeature=[](){return false;};
  services.postStream.extractQuotaStatusFromHeaders=[](const Json&){};
  services.extractQuotaStatusFromError=[](const batbox::retry::ErrorRecord&){};
  auto& p=services.preflight;
  p.isSubscriber=[](){return false;};
  p.isNonCustomPrimaryModel=[](const std::string&){return false;};
  p.offSwitchActivated=[](){return false;};
  p.getMergedBetas=[](const std::string&,bool){return Json::array();};
  p.isAdvisorEnabled=[](){return false;};
  p.isGadgetSearchEnabled=[](const std::string&,const Json&,const Json&){return false;};
  p.isDeferredGadget=[](const Json&){return false;};
  p.gadgetMatchesName=[](const Json&,const std::string&){return false;};
  p.isChromeGadget=[](const std::string&){return false;};
  p.getAPIProvider=[](){return std::string("local");};
  p.stashedMicrocompactFeature=[](){return false;};
  p.shouldUseGlobalStashScope=[](){return false;};
  p.getCLISyspromptPrefix=[](const Json&){return std::string{};};
  p.getHeaderLatch=[](const std::string&){return false;};
  p.transcriptClassifierFeature=[](){return false;};
  p.isFastModeEnabled=[](){return false;};
  p.isFastModeAvailable=[](){return true;};
  p.isFastModeCooldown=[](){return false;};
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
  b.isFastModeEnabled=[](){return false;};
  b.isFastModeAvailable=[](){return true;};
  b.isFastModeCooldown=[](){return false;};
  b.isFastModeSupportedByModel=[](const std::string&){return false;};
  b.normalizeModelStringForAPI=[](const std::string& model){return model;};
  services.headers.largeContext="large-context";
  services.retry.env={{"BATBOX_MAX_RETRIES","0"}};
  services.retry.isFastModeEnabled=[](){return false;};
  services.retry.logForDebugging=[](const std::string&){};
  services.retry.createSystemAPIErrorMessage=[](
      const batbox::retry::ErrorRecord&,double,double,double){
    return Json{{"type","system"},{"error","retry"}};};
  services.retry.sleep=[](double,std::stop_token){};
  services.retry.random=[](){return .5;};
  services.retry.nowMs=[](){return 0.0;};
  services.retry.isSubscriber=[](){return false;};
  services.retry.isDefaultPremiumModel=[](const std::string&){return false;};
  auto& reducer=services.reducer;
  reducer.nowMillis=[](){return std::int64_t{100};};
  reducer.randomUUID=[this](){return "uuid-"+std::to_string(++uuid);};
  reducer.timestamp=[](){return "2026-10-28T12:00:00.000Z";};
  reducer.calculateUSDCost=[](const std::string&,const Json&){return .1;};
  reducer.addToTotalFlightCost=[this](double cost,const Json&,
      const std::string&){++costCalls;return cost;};
  reducer.getErrorMessageIfRefusal=[](const Json&,
      const std::string&)->std::optional<Json>{return std::nullopt;};
  reducer.createAssistantAPIErrorMessage=[](const std::string&,
      const std::string&,const std::string&){
    return Json{{"type","assistant"},{"error","max_output_tokens"}};};
  services.createClient=[this]{
    batbox::transport::ClientConfig config;
    config.baseUrl=baseUrl;config.messagesPath="/v1/messages";
    config.requestTimeout=std::chrono::seconds(8);
    config.streamIdleTimeout=std::chrono::seconds(6);
    return std::make_shared<batbox::transport::Client>(config);
  };
  services.normalizeFallbackContent=[](const Json& content){
    return batbox::streaming::normalizeContentFromAPI(content);};
  services.getAssistantMessageFromError=[](const batbox::api_errors::Failure&,
      const std::string&){
    return Json{{"type","assistant"},{"error","stream-error"}};};
  services.onStreamingFallback=[this]{++fallbacks;};
  services.startFlightActivity=[](const std::string&){};
  services.stopFlightActivity=[](const std::string&){};
  // The fixture has no ambient BatContext. The completion branch
  // therefore stores server request IDs for sdk queries in main-loop state.
  services.hasBatContext=[](){return false;};
  services.setLastMainRequestId=[this](const std::string& id){
    lastMainRequestId=id;};
  services.nowMillis=[](){return std::int64_t{100};};
  services.environment={{"API_TIMEOUT_MS","8000"}};
  return services;
}
