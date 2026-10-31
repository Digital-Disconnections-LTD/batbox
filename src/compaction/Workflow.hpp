#pragma once
#include "compaction/Compaction.hpp"
#include "compaction/Poetry.hpp"
#include "messages/Context.hpp"
#include <future>
#include <memory>

namespace batbox::bats {struct StashSafeParams;}

namespace batbox::compaction {
struct Context {
  Json data;
  std::shared_ptr<bats::StashSafeParams> indigenousStashSafeParams;
  std::stop_token signal;
  std::function<Json()> getAppState;
  std::function<void(const Json&)> onCompactProgress;
  std::function<void(const Json&)> setSDKStatus;
  std::function<void(const std::string&)> setStreamMode;
  std::function<void(const std::function<double(double)>&)> setResponseLength;
  std::function<void(const Json&)> addNotification;
  std::function<Json()> snapshotReadFileState;
  std::function<void()> clearReadFileState;
  std::function<void()> clearLoadedNestedMemoryPaths;
};
struct SummaryInputs {
  Json messages,summaryRequest,appState,stashSafeParams;
  double preCompactTokenCount=0;
  std::shared_ptr<bats::StashSafeParams> indigenousStashSafeParams;
};
struct WorkflowServices {
  HelperServices helpers;
  messages::BoundaryServices boundary;
  PoetryServices poetry;
  std::function<double(const Json&)> tokenCountWithEstimation;
  std::function<Json(const Json&)> createUserMessage;
  std::function<Json(const Json&,std::stop_token)> executePreCompactHooks;
  std::function<Json(const SummaryInputs&,Context&)> streamCompactSummary;
  std::function<bool(const std::string&)> startsWithApiErrorPrefix;
  std::function<std::future<Json>(const Json&,Context&,double,const Json&)> createPostCompactFileAttachments;
  std::function<std::future<Json>(Context&)> createAsyncBatAttachmentsIfNeeded;
  std::function<Json(const Json&)> createPlanAttachmentIfNeeded;
  std::function<Json(Context&)> createPlanModeAttachmentIfNeeded;
  std::function<Json(const Json&)> createSkillAttachmentIfNeeded;
  std::function<Json(const Json&,const Json&,const Json&,const Json&)> getDeferredGadgetsDeltaAttachment;
  std::function<Json(Context&,const Json&)> getBatListingDeltaAttachment;
  std::function<Json(const Json&,const Json&,const Json&,const Json&)> getMcpInstructionsDeltaAttachment;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<Json(const std::string&,const Json&)> processFlightStartHooks;
  std::function<Json(const Json&)> extractDiscoveredGadgetNames;
  std::function<std::string()> getTranscriptPath;
  std::function<double(const Json&)> tokenCountFromLastAPIResponse;
  std::function<double(const Json&)> roughTokenCountEstimationForMessages;
  std::function<Json(const Json&)> getTokenUsage;
  std::function<void()> markPostCompaction;
  std::function<void()> reAppendFlightMetadata;
  std::function<bool(const std::string&)> feature;
  // Fire-and-forget binding retains local transcript work until completion.
  std::function<void(const Json&)> writeFlightTranscriptSegment;
  std::function<Json(const Json&,std::stop_token)> executePostCompactHooks;
};
Json compactConversation(const Json& messages,Context& context,const Json& stashSafeParams,
  bool suppressFollowUpQuestions,std::optional<std::string> customInstructions,bool isAutoCompact,
  const Json& recompactionInfo,WorkflowServices& services);
void addErrorNotificationIfNeeded(const std::string& error,Context& context);
Json createCompactCanUseGadget();
Json partialCompactConversation(const Json& allMessages,double pivot,Context& context,const Json& stashSafeParams,
  const std::optional<std::string>& userFeedback,const std::string& direction,WorkflowServices& services);
}
