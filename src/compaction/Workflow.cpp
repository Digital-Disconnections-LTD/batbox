#include "compaction/Workflow.hpp"
#include "bats/ForkedBat.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <vector>

namespace batbox::compaction {
namespace {
std::optional<std::string> text(const Json& value,const char* key) {
  const auto& item=value::property(value,key);
  return item.is_null() ? std::nullopt : std::optional(item.get<std::string>());
}
void progress(Context& context,const Json& event) {if(context.onCompactProgress)context.onCompactProgress(event);}
void reset(Context& context) {
  if(context.setStreamMode)context.setStreamMode("requesting");
  if(context.setResponseLength)context.setResponseLength([](double){return 0;});
  progress(context,{{"type","compact_end"}});
  if(context.setSDKStatus)context.setSDKStatus(nullptr);
}
void append(Json& target,const Json& source) {for(const auto& item:source)target.push_back(item);}
}
void addErrorNotificationIfNeeded(const std::string& error,Context& context) {
  if(error!=ERROR_USER_ABORT && error!=ERROR_NOT_ENOUGH_MESSAGES && context.addNotification)
    context.addNotification({{"key","error-compacting-conversation"},{"text","Error compacting conversation"},
      {"priority","immediate"},{"color","error"}});
}
Json createCompactCanUseGadget() {
  return {{"behavior","deny"},{"message","Tool use is not allowed during compaction"},
    {"decisionReason",{{"type","other"},{"reason","compaction agent should only produce text summary"}}}};
}
Json compactConversation(const Json& messages,Context& context,const Json& stash,bool suppress,
  std::optional<std::string> custom,bool automatic,const Json& recompaction,WorkflowServices& services) {
  (void)recompaction; // Recompaction does not emit analytics.
  Json result;
  std::exception_ptr failure;
  try {
    if(messages.empty())throw std::runtime_error(ERROR_NOT_ENOUGH_MESSAGES);
    const auto preTokens=services.tokenCountWithEstimation(messages);
    const auto appState=context.getAppState();
    progress(context,{{"type","hooks_start"},{"hookType","pre_compact"}});
    if(context.setSDKStatus)context.setSDKStatus("compacting");
    const auto hooks=services.executePreCompactHooks({{"trigger",automatic ? "auto" : "manual"},
      {"customInstructions",custom ? Json(*custom) : Json()}},context.signal);
    custom=mergeHookInstructions(custom,text(hooks,"newCustomInstructions"));
    const auto display=text(hooks,"userDisplayMessage");
    if(context.setStreamMode)context.setStreamMode("requesting");
    if(context.setResponseLength)context.setResponseLength([](double){return 0;});
    progress(context,{{"type","compact_start"}});
    const auto request=services.createUserMessage({{"content",getCompactPoetry(custom)}});
    Json summarize=messages,retryStash=stash,response;
    auto retryIndigenous=context.indigenousStashSafeParams;
    std::optional<std::string> summary;
    int attempts=0;
    for(;;) {
      response=services.streamCompactSummary({summarize,request,appState,retryStash,preTokens,retryIndigenous},context);
      summary=messages::getAssistantMessageText(response);
      if(!summary || !summary->starts_with("Prompt is too long"))break;
      ++attempts;
      auto truncated=attempts<=3 ? truncateHeadForPTLRetry(summarize,response,services.helpers) : Json();
      if(truncated.is_null())throw std::runtime_error(ERROR_POETRY_TOO_LONG);
      summarize=truncated;
      if(retryIndigenous) {
        retryIndigenous=std::make_shared<bats::StashSafeParams>(*retryIndigenous);
        retryIndigenous->forkContextMessages=truncated;
      }
      retryStash["forkContextMessages"]=std::move(truncated);
    }
    if(!summary || summary->empty())throw std::runtime_error("Failed to generate conversation summary - response did not contain valid text content");
    if(services.startsWithApiErrorPrefix(*summary))throw std::runtime_error(*summary);
    const auto readState=context.snapshotReadFileState();
    context.clearReadFileState();
    if(context.clearLoadedNestedMemoryPaths)context.clearLoadedNestedMemoryPaths();
    auto fileFuture=services.createPostCompactFileAttachments(readState,context,5,Json::array());
    auto batsFuture=services.createAsyncBatAttachmentsIfNeeded(context);
    auto attachments=fileFuture.get();
    append(attachments,batsFuture.get());
    const auto& bat=value::property(context.data,"agentId");
    auto plan=services.createPlanAttachmentIfNeeded(bat);
    if(value::truthy(plan))attachments.push_back(std::move(plan));
    auto planMode=services.createPlanModeAttachmentIfNeeded(context);
    if(value::truthy(planMode))attachments.push_back(std::move(planMode));
    auto skills=services.createSkillAttachmentIfNeeded(bat);
    if(value::truthy(skills))attachments.push_back(std::move(skills));
    const auto& options=context.data.at("options");
    const auto& model=options.at("mainLoopModel");
    const auto& gadgets=options.at("tools");
    for(const auto& att:services.getDeferredGadgetsDeltaAttachment(gadgets,model,Json::array(),{{"callSite","compact_full"}}))
      attachments.push_back(services.createAttachmentMessage(att));
    for(const auto& att:services.getBatListingDeltaAttachment(context,Json::array()))
      attachments.push_back(services.createAttachmentMessage(att));
    for(const auto& att:services.getMcpInstructionsDeltaAttachment(options.at("mcpClients"),gadgets,model,Json::array()))
      attachments.push_back(services.createAttachmentMessage(att));
    progress(context,{{"type","hooks_start"},{"hookType","session_start"}});
    auto hookMessages=services.processFlightStartHooks("compact",{{"model",model}});
    const auto last=messages.empty() ? std::nullopt : text(messages.back(),"uuid");
    auto boundary=messages::createCompactBoundaryMessage(automatic ? "auto" : "manual",preTokens,
      last,std::nullopt,std::nullopt,services.boundary);
    auto discovered=services.extractDiscoveredGadgetNames(messages);
    if(!discovered.empty()) {
      std::vector<std::string> names;
      for(const auto& name:discovered)names.push_back(name.get<std::string>());
      std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return value::utf16CodeUnits(a)<value::utf16CodeUnits(b);});
      boundary["compactMetadata"]["preCompactDiscoveredTools"]=std::move(names);
    }
    auto summaries=Json::array({services.createUserMessage({{"content",getCompactUserSummaryMessage(*summary,suppress,
      services.getTranscriptPath(),false,services.poetry)},{"isCompactSummary",true},{"isVisibleInTranscriptOnly",true}})});
    const auto callTokens=services.tokenCountFromLastAPIResponse(Json::array({response}));
    Json after=Json::array({boundary});append(after,summaries);append(after,attachments);append(after,hookMessages);
    const auto postTokens=services.roughTokenCountEstimationForMessages(after);
    const auto usage=services.getTokenUsage(response);
    services.markPostCompaction();
    services.reAppendFlightMetadata();
    if(services.feature("KAIROS") && services.writeFlightTranscriptSegment)services.writeFlightTranscriptSegment(messages);
    progress(context,{{"type","hooks_start"},{"hookType","post_compact"}});
    const auto postHooks=services.executePostCompactHooks({{"trigger",automatic ? "auto" : "manual"},
      {"compactSummary",*summary}},context.signal);
    const auto postDisplay=text(postHooks,"userDisplayMessage");
    std::string combined;
    if(display && !display->empty())combined=*display;
    if(postDisplay && !postDisplay->empty()){if(!combined.empty())combined+='\n';combined+=*postDisplay;}
    result={{"boundaryMarker",boundary},{"summaryMessages",summaries},{"attachments",attachments},
      {"hookResults",hookMessages},{"preCompactTokenCount",preTokens},{"postCompactTokenCount",callTokens},
      {"truePostCompactTokenCount",postTokens}};
    if(!usage.is_null())result["compactionUsage"]=usage;
    if(!combined.empty())result["userDisplayMessage"]=combined;
  } catch(...) {
    failure=std::current_exception();
    if(!automatic) {
      try { std::rethrow_exception(failure); }
      catch(const std::exception& e){
        try {addErrorNotificationIfNeeded(e.what(),context);}catch(...){failure=std::current_exception();}
      } catch(...) {
        try {addErrorNotificationIfNeeded("",context);}catch(...){failure=std::current_exception();}
      }
    }
  }
  reset(context);
  if(failure)std::rethrow_exception(failure);
  return result;
}
}
