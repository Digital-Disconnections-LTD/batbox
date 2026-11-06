#pragma once
#include "compaction/Workflow.hpp"
#include "model/ModelWrappers.hpp"
namespace batbox::compaction {
struct SummaryModelRequest {
  Json messages,systemPoetry,thinkingConfig,gadgets,options;
  std::stop_token signal;
  std::function<Json()> getGadgetPermissionContext;
};
struct SummaryServices {
  std::function<bool()> poetryStashSharingEnabled;
  std::function<bool()> streamingRetryEnabled;
  // Requests for the summary without the shared prefix after the fork fails;
  // unbound: 2 with streamingRetryEnabled, else 1.
  std::function<int()> summaryAttempts;
  // Why the shared-prefix fork failed before the fallback request.
  std::function<void(const std::string&)> log;
  std::function<Json(const Json&,std::stop_token,const std::function<Json()>&,
    const std::shared_ptr<bats::StashSafeParams>&)> runForkedBat;
  std::function<bool(const Json&,const Json&,const std::function<Json()>&,const Json&,const std::string&)> isGadgetSearchEnabled;
  Json fileReadGadget,gadgetSearchGadget;
  std::function<Json(const Json&,const Json&)> normalizeMessagesForAPI;
  std::function<double(const std::string&)> getMaxOutputTokensForModel;
  std::function<model::NextMessage(const SummaryModelRequest&)> queryModelWithStreaming;
  std::function<double(double)> getRetryDelay;
  std::function<void(double,std::stop_token)> sleep;
};
Json streamCompactSummary(const SummaryInputs& inputs,Context& context,SummaryServices& services);
}
