#pragma once
#include "query/Completion.hpp"
#include "execution/Orchestration.hpp"

namespace batbox::query {
struct MemoryPrefetch {
  std::function<bool()> isSettled;
  std::function<Json()> read;
  int consumedOnIteration=-1;
  std::function<void()> dispose;
};
struct GadgetTurnServices {
  execution::Services* executionServices=nullptr;
  std::function<Json(const Json&,const gadgets::Gadgets&)> normalizeMessagesForAPI;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<Json(bool gadgetUse)> createUserInterruptionMessage;
  std::function<std::string()> currentAbortReason;
  std::function<void(const execution::Context&)> cleanupComputerUseAfterTurn;
  std::function<bool(const std::string&)> feature;
  std::function<std::shared_future<std::optional<std::string>>(const Json&)> generateGadgetUseSummary;
  std::function<Json(const std::string&,const Json& gadgetUseIds)> createGadgetUseSummaryMessage;
  std::function<Json(const std::string& priority)> getCommandsByMaxPriority;
  std::function<bool(const Json&)> isSlashCommand;
  std::function<Json(const Json& queuedCommands,const execution::Context&,
                     const Json& messages,const std::string& querySource)> getAttachmentMessages;
  std::function<Json(const Json& attachments,const Json& readFileState)> filterDuplicateMemoryAttachments;
  std::function<Json(const Json& pendingSkillPrefetch)> collectSkillDiscoveryPrefetch;
  std::function<void(const std::string&,const std::string&)> notifyCommandLifecycle;
  std::function<void(const Json&)> removeFromQueue;
  std::function<std::optional<gadgets::Gadgets>(execution::Context&)> refreshGadgets;
  std::function<bool()> shouldGenerateMissionSummary;
  std::function<void(const Json&)> maybeGenerateMissionSummary;
  std::string sleepGadgetName="Sleep";
  std::string concurrencyValue;
};

CompletionDecision executeGadgetTurn(const Params& params,const State& state,
  PreparedIteration& prepared,ModelPassResult& modelResult,
  const QueryConfig& config,MemoryPrefetch* memoryPrefetch,
  std::vector<std::string>& consumedCommandUuids,GadgetTurnServices& services,
  const Yield& yield);
} // namespace batbox::query
