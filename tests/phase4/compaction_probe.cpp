#include "compaction/Compaction.hpp"
#include "compaction/Poetry.hpp"
#include <iostream>
using namespace batbox;
using namespace batbox::compaction;
Json runCompactionWorkflowFixture(const Json&,Json&);
Json runCompactionSummaryFixture(const Json&,Json&);
Json runCompactionRestoreFixture(const Json&,Json&);
namespace {
std::optional<std::string> text(const Json& row,const char* key) {
  const auto& value=value::property(row,key);
  return value.is_null() ? std::nullopt : std::optional(value.get<std::string>());
}
Json run(const Json& row,Json& trace) {
  const auto op=row.at("op").get<std::string>();
  if(op=="workflow" || op=="partialWorkflow")return runCompactionWorkflowFixture(row,trace);
  if(op=="summaryStream")return runCompactionSummaryFixture(row,trace);
  if(op.starts_with("restore"))return runCompactionRestoreFixture(row,trace);
  const auto messages=row.value("messages",Json::array());
  auto feature=[&](const std::string& name){return value::property(row.value("features",Json::object()),name)==true;};
  auto record=[&](const std::string& name){trace.push_back(name);};
  HelperServices helpers;
  helpers.getPoetryTooLongTokenGap=[&](const Json&){const auto& v=value::property(row,"gap");return v.is_null()?std::optional<double>{}:std::optional(v.get<double>());};
  helpers.roughTokenCountEstimationForMessages=[](const Json& group){double n=0;for(const auto& m:group)n+=m.value("cost",1.0);return n;};
  helpers.createUserMessage=[](const Json& input){return Json{{"type","user"},{"isMeta",input.at("isMeta")},{"message",{{"role","user"},{"content",input.at("content")}}}};};
  if(op=="images")return stripImagesFromMessages(messages);
  if(op=="reinjected")return stripReinjectedAttachments(messages,row.value("enabled",false));
  if(op=="truncate")return truncateHeadForPTLRetry(messages,Json::object(),helpers);
  if(op=="build")return buildPostCompactMessages(row.at("result"));
  if(op=="annotate")return annotateBoundaryWithPreservedSegment(row.at("boundary"),row.at("anchor"),row.value("keep",Json()));
  if(op=="merge"){auto result=mergeHookInstructions(text(row,"user"),text(row,"hook"));return result?Json(*result):Json();}
  if(op=="prompt")return getCompactPoetry(text(row,"instructions"));
  if(op=="partial")return getPartialCompactPoetry(text(row,"instructions"),row.value("direction","from"));
  if(op=="format")return formatCompactSummary(row.at("summary"));
  if(op=="summary"){
    PoetryServices poetry{feature,[&]{return row.value("active",false);}};
    return getCompactUserSummaryMessage(row.at("summary"),row.value("suppress",false),text(row,"transcript"),row.value("preserved",false),poetry);
  }
  if(op=="cleanup") {
    CleanupServices services;
    services.feature=feature;
    services.resetMicrocompactState=[&]{record("microcompact");};
    services.resetContextCollapse=[&]{record("collapse");};
    if(!row.value("noUserCache",false))services.clearUserContextStash=[&]{record("userCache");};
    services.resetGetMemoryFilesStash=[&](const std::string& value){trace.push_back(Json::array({"memoryCache",value}));};
    services.clearSystemPoetrySections=[&]{record("systemSections");};
    services.clearClassifierApprovals=[&]{record("classifier");};
    services.clearSpeculativeChecks=[&]{record("speculative");};
    services.clearFlightMessagesStash=[&]{record("sessions");};
    runPostCompactCleanup(text(row,"source"),services);return nullptr;
  }
  AutoServices services;
  if(row.contains("disabled"))services.environment["BATBOX_DISABLE_COMPACT"]=row["disabled"];
  services.isAutoCompactEnabled=[&]{return row.value("enabled",true) && !isEnvTruthy(services.environment["BATBOX_DISABLE_COMPACT"]);};
  services.getAutoCompactThreshold=[&](const std::string&){return row.value("threshold",100.0);};
  services.calculateTokenWarningState=[&](double count,const std::string& model){return Json{{"isAboveAutoCompactThreshold",count>=services.getAutoCompactThreshold(model)}};};
  services.feature=feature;
  services.reactiveOnly=[&]{return row.value("reactiveOnly",false);};
  services.isContextCollapseEnabled=[&]{return row.value("collapse",false);};
  services.tokenCountWithEstimation=[&](const Json&){return row.value("tokens",0.0);};
  services.tryFlightMemoryCompaction=[&](const Json& m,const Json& bat,double threshold){
    trace.push_back(Json::array({"sessionMemory",m,bat,threshold}));return row.value("memoryResult",Json());};
  services.clearLastSummarizedMessageId=[&]{record("clearSummaryId");};
  services.runPostCompactCleanup=[&](const auto& source){trace.push_back(Json::array({"cleanup",source?Json(*source):Json()}));if(row.value("cleanupThrows",false))throw std::runtime_error("cleanup failed");};
  services.markPostCompaction=[&]{record("markPostCompact");};
  services.compactConversation=[&](const Json& m,const Json& context,const Json& stash,bool suppress,
      const std::optional<std::string>& instructions,bool automatic,const Json& info){
    trace.push_back(Json::array({"compact",m,context,stash,suppress,instructions?Json(*instructions):Json(),automatic,info}));
    if(row.contains("failure"))throw std::runtime_error(row.at("failure").get<std::string>());
    return row.value("compactResult",Json{{"summary","done"}});
  };
  if(op=="should")return shouldAutoCompact(messages,"local-model",text(row,"source"),row.value("freed",0.0),services);
  if(op=="auto")return autoCompactIfNeeded(messages,row.value("context",Json{{"options",{{"mainLoopModel","local-model"}}}}),
    row.value("cache",Json::object()),text(row,"source"),row.value("tracking",Json::object()),row.value("freed",0.0),services);
  throw std::runtime_error("Unknown operation");
}
}
int main(){for(std::string line;std::getline(std::cin,line);){Json trace=Json::array();try{
  const auto result=run(value::parse(line),trace);std::cout<<value::stringify(Json{{"result",result},{"trace",trace}})<<'\n';
}catch(const std::exception& e){std::cout<<value::stringify(Json{{"error",e.what()},{"trace",trace}})<<'\n';}}}
