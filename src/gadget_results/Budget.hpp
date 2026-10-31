#pragma once
#include "gadget_results/State.hpp"
#include "gadget_results/Storage.hpp"

namespace batbox::gadget_results {
using MessageRef=std::shared_ptr<Json>;
using MessageArray=std::shared_ptr<std::vector<MessageRef>>;
struct Candidate {std::string gadgetUseId;Json content;double size=0;};
struct ReapplyCandidate:Candidate {std::string replacement;};
struct CandidatePartition {
  std::vector<ReapplyCandidate> mustReapply;
  std::vector<Candidate> frozen,fresh;
};
std::unordered_map<std::string,std::string> buildGadgetNameMap(const Json& messages);
std::vector<Candidate> collectCandidatesFromMessage(const Json& message);
std::vector<std::vector<Candidate>> collectCandidatesByMessage(const Json& messages);
CandidatePartition partitionByPriorDecision(const std::vector<Candidate>&,const ContentReplacementState&);
std::vector<Candidate> selectFreshToReplace(const std::vector<Candidate>&,double frozenSize,double limit);
MessageArray makeMessageArray(const Json& messages);
Json snapshotMessages(const MessageArray&);
MessageArray replaceGadgetResultContents(const MessageArray&,const ReplacementMap&);
Json replaceGadgetResultContents(const Json&,const ReplacementMap&);
std::shared_ptr<ContentReplacementState> reconstructContentReplacementState(
  const Json& messages,const Json& records,const ReplacementMap* inherited=nullptr);
std::shared_ptr<ContentReplacementState> reconstructForSubagentResume(
  const std::shared_ptr<ContentReplacementState>& parent,const Json& messages,const Json& records);
std::shared_ptr<ContentReplacementState> provisionContentReplacementState(bool enabled,
  const std::optional<Json>& initialMessages={},const Json& records=Json::array());
struct BudgetImports {
  StorageImports storage;
  Json limitOverride;
  // Defaults to Indigenous persistGadgetResult. Injection preserves the async
  // boundary for deterministic filesystem and failure-path tests.
  std::function<event_loop::Deferred<Json>(const Json&,const std::string&)> persist;
};
struct EnforcementResult {MessageArray messages;Json newlyReplaced=Json::array();};
event_loop::Deferred<Json> buildReplacement(event_loop::Loop&,const BudgetImports&,const Candidate&);
event_loop::Deferred<EnforcementResult> enforceGadgetResultBudget(event_loop::Loop&,
  const BudgetImports&,const MessageArray&,const std::shared_ptr<ContentReplacementState>&,
  const std::unordered_set<std::string>& skipGadgetNames={});
event_loop::Deferred<MessageArray> applyGadgetResultBudget(event_loop::Loop&,
  const BudgetImports&,const MessageArray&,const std::shared_ptr<ContentReplacementState>&,
  const std::function<void(const Json&)>& writeToTranscript={},
  const std::unordered_set<std::string>& skipGadgetNames={});
} // namespace batbox::gadget_results
