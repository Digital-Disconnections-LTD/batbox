#include "compaction/Workflow.hpp"
#include "bats/ForkedBat.hpp"
#include <stdexcept>
using namespace batbox;
using namespace batbox::compaction;
Json runCompactionWorkflowFixture(const Json& row,Json& trace) {
  auto record=[&](const std::string& name){trace.push_back(name);};
  auto ready=[](Json value){std::promise<Json> promise;promise.set_value(std::move(value));return promise.get_future();};
  auto createUser=[](const Json& input){
    Json result={{"type","user"},{"message",{{"role","user"},{"content",input.at("content")}}}};
    for(const auto key:{"isMeta","isCompactSummary","isVisibleInTranscriptOnly","summarizeMetadata"})
      if(input.contains(key))result[key]=input[key];
    return result;
  };
  Context context;
  context.data=row.value("context",Json{{"agentId","worker"},{"options",{{"tools",Json::array()},
    {"mcpClients",Json::array()},{"mainLoopModel","local-model"}}}});
  if(row.value("nativeCache",false)) {
    context.indigenousStashSafeParams=std::make_shared<bats::StashSafeParams>();
    context.indigenousStashSafeParams->gadgetUseContext.getAppState=[]{return Json{{"callback","retained"}};};
    context.indigenousStashSafeParams->forkContextMessages=row.at("messages");
  }
  context.getAppState=[&]{record("appState");return Json{{"toolPermissionContext",{{"mode","default"}}}};};
  context.onCompactProgress=[&](const Json& event){trace.push_back(Json::array({"progress",event}));};
  context.setSDKStatus=[&](const Json& status){trace.push_back(Json::array({"status",status}));};
  context.setStreamMode=[&](const std::string& value){trace.push_back(Json::array({"mode",value}));};
  double length=0;
  context.setResponseLength=[&](const auto& change){length=change(length);trace.push_back(Json::array({"length",length}));};
  context.addNotification=[&](const Json& value){trace.push_back(Json::array({"notification",value}));};
  context.snapshotReadFileState=[&]{record("snapshot");return row.value("reads",Json::object());};
  context.clearReadFileState=[&]{record("clearReads");};
  context.clearLoadedNestedMemoryPaths=[&]{record("clearNested");};
  WorkflowServices services;
  services.helpers.getPoetryTooLongTokenGap=[&](const Json&){return row.contains("gap") ? std::optional(row.at("gap").get<double>()) : std::nullopt;};
  services.helpers.roughTokenCountEstimationForMessages=[](const Json& messages){double n=0;for(const auto& m:messages)n+=m.value("cost",1.0);return n;};
  services.helpers.createUserMessage=createUser;
  services.tokenCountWithEstimation=[&](const Json&){record("tokenCount");return row.value("tokens",150.0);};
  services.createUserMessage=createUser;
  services.executePreCompactHooks=[&](const Json& value,std::stop_token){
    trace.push_back(Json::array({"preHooks",value}));
    if(row.contains("preHookFailure"))throw std::runtime_error(row.at("preHookFailure").get<std::string>());
    return row.value("preHooks",Json::object());
  };
  std::size_t attempt=0;
  services.streamCompactSummary=[&](const SummaryInputs& input,Context&){
    if(row.value("nativeCache",false)) {
      const auto snapshot=input.indigenousStashSafeParams;
      if(!snapshot || !snapshot->gadgetUseContext.getAppState ||
          snapshot->gadgetUseContext.getAppState()!=Json{{"callback","retained"}})
        throw std::runtime_error("Compaction lost native parent callbacks");
      const auto expected=input.stashSafeParams.value("forkContextMessages",row.at("messages"));
      if(snapshot->forkContextMessages!=expected)throw std::runtime_error("Compaction lost typed fork-message override");
    }
    trace.push_back(Json::array({"summary",input.messages,input.summaryRequest,input.stashSafeParams}));
    if(row.contains("summaryFailure"))throw std::runtime_error(row.at("summaryFailure").get<std::string>());
    const auto responses=row.value("summaries",Json::array({"<analysis>draft</analysis><summary>remembered</summary>"}));
    const auto value=responses[std::min(attempt++,responses.size()-1)];
    return Json{{"type","assistant"},{"message",{{"content",value.is_null()?Json::array():Json::array({Json{{"type","text"},{"text",value}}})}}}};
  };
  services.startsWithApiErrorPrefix=[](const std::string& value){return value.starts_with("API Error:");};
  services.createPostCompactFileAttachments=[&](const Json& reads,Context&,double max,const Json& preserved){
    auto call=Json::array({"files",reads,max});if(row.at("op")=="partialWorkflow")call.push_back(preserved);
    trace.push_back(call);return ready(row.value("files",Json::array()));};
  services.createAsyncBatAttachmentsIfNeeded=[&](Context&){record("agents");return ready(row.value("agents",Json::array()));};
  services.createPlanAttachmentIfNeeded=[&](const Json& bat){trace.push_back(Json::array({"plan",bat}));return row.value("plan",Json());};
  services.createPlanModeAttachmentIfNeeded=[&](Context&){record("planMode");return row.value("planMode",Json());};
  services.createSkillAttachmentIfNeeded=[&](const Json& bat){trace.push_back(Json::array({"skills",bat}));return row.value("skills",Json());};
  services.getDeferredGadgetsDeltaAttachment=[&](const Json&,const Json&,const Json&,const Json&){record("deferred");return row.value("deferred",Json::array());};
  services.getBatListingDeltaAttachment=[&](Context&,const Json&){record("listing");return row.value("listing",Json::array());};
  services.getMcpInstructionsDeltaAttachment=[&](const Json&,const Json&,const Json&,const Json&){record("mcpInstructions");return row.value("mcpInstructions",Json::array());};
  services.createAttachmentMessage=[](const Json& att){return Json{{"type","attachment"},{"attachment",att}};};
  services.processFlightStartHooks=[&](const std::string& kind,const Json& opts){trace.push_back(Json::array({"sessionHooks",kind,opts}));return row.value("sessionHooks",Json::array());};
  services.boundary.nowISO=[] {return "2026-10-28T00:00:00.000Z";};
  services.boundary.randomUUID=[] {return "boundary-uuid";};
  services.extractDiscoveredGadgetNames=[&](const Json&){record("discovered");return row.value("discovered",Json::array());};
  services.getTranscriptPath=[&]{record("transcript");return "/tmp/session.jsonl";};
  services.tokenCountFromLastAPIResponse=[&](const Json&){record("apiTokens");return 17;};
  services.roughTokenCountEstimationForMessages=services.helpers.roughTokenCountEstimationForMessages;
  services.getTokenUsage=[&](const Json&){record("usage");return row.value("usage",Json{{"input_tokens",10},{"output_tokens",7}});};
  services.markPostCompaction=[&]{record("markPostCompact");};
  services.reAppendFlightMetadata=[&]{record("metadata");};
  services.feature=[&](const std::string& name){return value::property(row.value("features",Json::object()),name)==true;};
  services.writeFlightTranscriptSegment=[&](const Json&){record("segment");};
  services.poetry={services.feature,[&]{return row.value("active",false);}};
  services.executePostCompactHooks=[&](const Json& value,std::stop_token){
    trace.push_back(Json::array({"postHooks",value}));
    if(row.contains("postHookFailure"))throw std::runtime_error(row.at("postHookFailure").get<std::string>());
    return row.value("postHooks",Json::object());
  };
  std::optional<std::string> instructions;
  if(row.contains("instructions")&&!row["instructions"].is_null())instructions=row["instructions"];
  auto result=row.at("op")=="partialWorkflow" ? partialCompactConversation(row.value("messages",Json::array()),
    row.value("pivot",0.0),context,row.value("cache",Json::object()),instructions,row.value("direction","from"),services) :
    compactConversation(row.value("messages",Json::array()),context,row.value("cache",Json::object()),
      row.value("suppress",true),instructions,row.value("automatic",false),row.value("recompaction",Json()),services);
  if(context.indigenousStashSafeParams && context.indigenousStashSafeParams->forkContextMessages!=row.at("messages"))
    throw std::runtime_error("Compaction mutated the parent cache snapshot");
  return result;
}
