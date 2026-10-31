#pragma once
#include "bats/BatMissionProgress.hpp"
#include "runtime/MessageSnapshot.hpp"

namespace batbox::bats {
struct BatGadgetForegroundProgressServices {
  std::function<Json(const Json&)> normalizeMessages;
  std::function<void(Json&,const Json&,const ActivityResolver&,
    const gadgets::Gadgets&)> updateProgressFromMessage;
  std::function<void(const Json&)> onProgress;
  std::function<void(const Json&,const std::string& missionId,
    const Json& gadgetUseId,const std::string& description,
    std::int64_t startTime,const std::string& lastGadgetName)> emitMissionProgress;
  std::function<bool()> getSdkBatProgressSummariesEnabled;
  std::function<Json(const Json&)> getProgressUpdate;
  std::function<void(const std::string& missionId,const Json& progress,
    const RootSetAppState&)> updateAsyncBatProgress;
  std::function<double(const Json&)> getAssistantMessageContentLength;
};
void emitInitialBatGadgetProgress(const Json& poetryMessages,
  const std::string& poetry,const std::string& syncBatId,
  const Json& assistantMessage,BatGadgetForegroundProgressServices& services);
void processBatGadgetForegroundMessage(const query::MessageRef& ref,
  BatMessageRefs& messages,Json& tracker,const ActivityResolver& resolveActivity,
  const gadgets::Gadgets& parentGadgets,const std::optional<std::string>& foregroundMissionId,
  const Json& gadgetUseId,const std::string& description,std::int64_t startTime,
  const std::string& poetry,const std::string& syncBatId,
  const Json& assistantMessage,const RootSetAppState& rootSetAppState,
  const BatContext& context,BatGadgetForegroundProgressServices& services);
} // namespace batbox::bats
