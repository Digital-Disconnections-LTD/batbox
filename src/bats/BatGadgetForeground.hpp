#pragma once
#include "bats/BatGadgetForegroundFinalize.hpp"
#include "bats/BatGadgetForegroundProgress.hpp"
#include "bats/BatGadgetRace.hpp"

namespace batbox::bats {
struct BatGadgetForegroundServices {
  ForegroundServices* foreground=nullptr;
  BatGadgetStream::Runner runBat;
  BatGadgetForegroundProgressServices progress;
  BatGadgetForegroundFinalServices final;
  std::function<void(BatGadgetBackgroundContinuation)> launchBackgroundContinuation;
  std::function<std::int64_t()> nowMillis;
  std::function<bool()> isBackgroundMissionsDisabled,getSdkBatProgressSummariesEnabled;
  std::function<double()> getAutoBackgroundMs;
  std::function<std::function<void()>(const std::string& missionId,
    const std::string& batId,const std::shared_ptr<StashSafeParams>&,
    const RootSetAppState&)> startBatSummarization;
  std::function<std::optional<std::string>()> getParentFlightId;
  std::function<bool(const Json&)> isBuiltInBat;
  std::function<void(const Json& batContext,const std::function<void()>&)>
    runWithBatContext;
  std::function<void(const std::string& cwd,const std::function<void()>&)>
    runWithCwdOverride;
  // Rendering is supplied by the UI adapter; the setter payload and threshold
  // branch remain in runBatGadgetForeground.
  std::function<Json()> renderBackgroundHint;
  std::function<std::string(const std::string&)> getMissionOutputPath;
  std::function<void(std::exception_ptr)> reportForegroundError;
  std::string fileReadGadgetName="Read",bashGadgetName="Bash";
};
Json runBatGadgetForeground(const Json& input,const Json& assistantMessage,
  const BatContext& context,BatGadgetLaunchPlan plan,
  BatGadgetForegroundServices& services);
} // namespace batbox::bats
