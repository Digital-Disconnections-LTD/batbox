#pragma once
#include "runtime/JsonValue.hpp"
#include <cstddef>
#include <functional>
#include <string>

namespace batbox::compaction {

// What a flight is doing, decided from its querySource. The classes are the
// extension slots: a future agent tier (less capable than the orchestrator
// but complex-task capable) gets its own row here and its own policy, with
// no re-plumbing of the compaction call path.
enum class FlightKind {
  Main,      // interactive session with a user: the summary template fits.
  Dispatch,  // launched task thread, reports to a dispatcher: no user, no
             // conversation to continue; summarize-into-self is the wrong
             // operation when the thread is weaker than the task.
  Auxiliary, // one-shot machinery turns (compact, session_memory).
};
FlightKind flightKind(const std::string& querySource);

struct FlightPolicy {
  FlightKind kind=FlightKind::Main;
  // The LLM summary pass may run automatically on this flight.
  bool llmAutoCompact=true;
  // Deterministic tool-result pruning instead: fold older results to one
  // line when the thread exceeds pruneFraction of the endpoint context.
  bool budgetMicrocompact=false;
  std::size_t keepRecentToolResults=3;
  double pruneFraction=0.75;
};
// Unknown sources behave as Main: only classes listed explicitly here ever
// change behavior against the historical default.
const FlightPolicy& flightPolicy(const std::string& querySource);

struct PruneResult {
  Json messages=Json::array();
  std::size_t cleared=0;
  double tokensSaved=0;
  bool changed=false;
};
// Folds every compactable tool result except the last keepRecent ones to
// `folded`. resultTokens estimates one tool_result block; pure function --
// never touches message order, assistant text, or the kept results.
PruneResult pruneOldToolResults(const Json& messages,std::size_t keepRecent,
  const std::function<double(const Json&)>& resultTokens,
  const std::string& folded="[Old tool result content cleared]");

} // namespace batbox::compaction
