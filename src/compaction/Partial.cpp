#include "compaction/Workflow.hpp"
#include "bats/ForkedBat.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <vector>
namespace batbox::compaction {
namespace {
std::optional<std::string> text(const Json& value,const char* key) {
  const auto& item=value::property(value,key);
  return item.is_null() ? std::nullopt : std::optional(item.get<std::string>());
}
void progress(Context& context,const Json& event){if(context.onCompactProgress)context.onCompactProgress(event);}
void append(Json& target,const Json& values){for(const auto& value:values)target.push_back(value);}
}
Json partialCompactConversation(const Json& all,double pivot,Context& context,const Json& stash,
  const std::optional<std::string>& feedback,const std::string& direction,WorkflowServices& services) {
  std::exception_ptr failure;Json result;
  try {
    const double position=std::isnan(pivot) ? 0 : std::trunc(pivot);
    const auto split=static_cast<std::ptrdiff_t>(position<0 ? std::max(0.0,all.size()+position) : std::min<double>(all.size(),position));
    const bool up=direction=="up_to";
    const Json summarized=up ? Json(all.begin(),all.begin()+split) : Json(all.begin()+split,all.end());
    const Json candidate=up ? Json(all.begin()+split,all.end()) : Json(all.begin(),all.begin()+split);
    Json kept=Json::array();
    for(const auto& m:candidate) {
      if(m.at("type")=="progress")continue;
      if(up && (messages::isCompactBoundaryMessage(m) ||
          (m.at("type")=="user" && value::truthy(value::property(m,"isCompactSummary")))))continue;
      kept.push_back(m);
    }
    if(summarized.empty())throw std::runtime_error(up ? "Nothing to summarize before the selected message." : "Nothing to summarize after the selected message.");
    const auto preTokens=services.tokenCountWithEstimation(all);
    progress(context,{{"type","hooks_start"},{"hookType","pre_compact"}});
    if(context.setSDKStatus)context.setSDKStatus("compacting");
    const auto preHooks=services.executePreCompactHooks({{"trigger","manual"},{"customInstructions",nullptr}},context.signal);
    auto custom=text(preHooks,"newCustomInstructions");
    if(feedback && !feedback->empty())custom=custom && !custom->empty() ? *custom+"\n\nUser context: "+*feedback : "User context: "+*feedback;
    if(context.setStreamMode)context.setStreamMode("requesting");
    if(context.setResponseLength)context.setResponseLength([](double){return 0;});
    progress(context,{{"type","compact_start"}});
    const auto request=services.createUserMessage({{"content",getPartialCompactPoetry(custom,direction)}});
    Json apiMessages=up ? summarized : all,retryStash=stash,response;
    auto retryIndigenous=context.indigenousStashSafeParams;
    if(up)retryStash["forkContextMessages"]=summarized;
    if(up && retryIndigenous) {
      retryIndigenous=std::make_shared<bats::StashSafeParams>(*retryIndigenous);
      retryIndigenous->forkContextMessages=summarized;
    }
    std::optional<std::string> summary;int attempts=0;
    for(;;) {
      response=services.streamCompactSummary({apiMessages,request,context.getAppState(),retryStash,preTokens,retryIndigenous},context);
      summary=messages::getAssistantMessageText(response);
      if(!summary || !summary->starts_with("Prompt is too long"))break;
      ++attempts;
      auto truncated=attempts<=3 ? truncateHeadForPTLRetry(apiMessages,response,services.helpers) : Json();
      if(truncated.is_null())throw std::runtime_error(ERROR_POETRY_TOO_LONG);
      apiMessages=truncated;
      if(retryIndigenous) {
        retryIndigenous=std::make_shared<bats::StashSafeParams>(*retryIndigenous);
        retryIndigenous->forkContextMessages=truncated;
      }
      retryStash["forkContextMessages"]=std::move(truncated);
    }
    if(!summary || summary->empty())throw std::runtime_error("Failed to generate conversation summary - response did not contain valid text content");
    if(services.startsWithApiErrorPrefix(*summary))throw std::runtime_error(*summary);
    const auto reads=context.snapshotReadFileState();context.clearReadFileState();
    if(context.clearLoadedNestedMemoryPaths)context.clearLoadedNestedMemoryPaths();
    auto files=services.createPostCompactFileAttachments(reads,context,5,kept);
    auto bats=services.createAsyncBatAttachmentsIfNeeded(context);
    auto attachments=files.get();append(attachments,bats.get());
    const auto& bat=value::property(context.data,"agentId");
    auto plan=services.createPlanAttachmentIfNeeded(bat);if(value::truthy(plan))attachments.push_back(plan);
    auto mode=services.createPlanModeAttachmentIfNeeded(context);if(value::truthy(mode))attachments.push_back(mode);
    auto skill=services.createSkillAttachmentIfNeeded(bat);if(value::truthy(skill))attachments.push_back(skill);
    const auto& options=context.data.at("options");
    const auto& gadgets=options.at("tools");const auto& model=options.at("mainLoopModel");
    for(const auto& att:services.getDeferredGadgetsDeltaAttachment(gadgets,model,kept,{{"callSite","compact_partial"}}))
      attachments.push_back(services.createAttachmentMessage(att));
    for(const auto& att:services.getBatListingDeltaAttachment(context,kept))attachments.push_back(services.createAttachmentMessage(att));
    for(const auto& att:services.getMcpInstructionsDeltaAttachment(options.at("mcpClients"),gadgets,model,kept))
      attachments.push_back(services.createAttachmentMessage(att));
    progress(context,{{"type","hooks_start"},{"hookType","session_start"}});
    auto hooks=services.processFlightStartHooks("compact",{{"model",model}});
    const auto postTokens=services.tokenCountFromLastAPIResponse(Json::array({response}));
    const auto usage=services.getTokenUsage(response);
    std::optional<std::string> last;
    if(up) {
      for(auto i=split;i>0;--i)if(all[i-1].at("type")!="progress"){last=text(all[i-1],"uuid");break;}
    }else if(!kept.empty())last=text(kept.back(),"uuid");
    auto boundary=messages::createCompactBoundaryMessage("manual",preTokens,last,feedback,
      static_cast<double>(summarized.size()),services.boundary);
    const auto discovered=services.extractDiscoveredGadgetNames(all);
    if(!discovered.empty()) {
      std::vector<std::string> names;
      for(const auto& name:discovered)names.push_back(name.get<std::string>());
      std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return value::utf16CodeUnits(a)<value::utf16CodeUnits(b);});
      boundary["compactMetadata"]["preCompactDiscoveredTools"]=std::move(names);
    }
    Json args={{"content",getCompactUserSummaryMessage(*summary,false,services.getTranscriptPath(),false,services.poetry)},
      {"isCompactSummary",true}};
    if(!kept.empty()) {
      args["summarizeMetadata"]={{"messagesSummarized",summarized.size()},{"direction",direction}};
      if(feedback)args["summarizeMetadata"]["userContext"]=*feedback;
    }else args["isVisibleInTranscriptOnly"]=true;
    auto summaries=Json::array({services.createUserMessage(args)});
    services.markPostCompaction();services.reAppendFlightMetadata();
    if(services.feature("KAIROS") && services.writeFlightTranscriptSegment)services.writeFlightTranscriptSegment(summarized);
    progress(context,{{"type","hooks_start"},{"hookType","post_compact"}});
    const auto postHooks=services.executePostCompactHooks({{"trigger","manual"},{"compactSummary",*summary}},context.signal);
    auto anchor=up ? text(summaries.back(),"uuid") : text(boundary,"uuid");
    if(!anchor)anchor=text(boundary,"uuid");
    result={{"boundaryMarker",annotateBoundaryWithPreservedSegment(boundary,*anchor,kept)},
      {"summaryMessages",summaries},{"messagesToKeep",kept},{"attachments",attachments},{"hookResults",hooks},
      {"preCompactTokenCount",preTokens},{"postCompactTokenCount",postTokens}};
    if(postHooks.contains("userDisplayMessage"))result["userDisplayMessage"]=postHooks["userDisplayMessage"];
    if(!usage.is_null())result["compactionUsage"]=usage;
  }catch(...) {
    failure=std::current_exception();
    try{std::rethrow_exception(failure);}catch(const std::exception& e){
      try{addErrorNotificationIfNeeded(e.what(),context);}catch(...){failure=std::current_exception();}
    }catch(...){try{addErrorNotificationIfNeeded("",context);}catch(...){failure=std::current_exception();}}
  }
  if(context.setStreamMode)context.setStreamMode("requesting");
  if(context.setResponseLength)context.setResponseLength([](double){return 0;});
  progress(context,{{"type","compact_end"}});
  if(context.setSDKStatus)context.setSDKStatus(nullptr);
  if(failure)std::rethrow_exception(failure);
  return result;
}
}
