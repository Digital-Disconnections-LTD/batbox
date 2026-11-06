#pragma once
#include "query/ModelPass.hpp"

namespace batbox::query {
struct CompletionServices {
  CreateUserMessage createUserMessage;
  std::function<void(const Json&,const execution::Context&)> executeStopFailureHooks;
  std::function<Json(const Json& messagesForQuery,const Json& assistantMessages,
                     const Params&,const execution::Context&,const std::optional<bool>& stopHookActive,
                     const Yield&)> handleStopHooks;
};
struct CompletionDecision {
  std::optional<State> next;
  std::optional<Json> terminal;
};

CompletionDecision completeWithoutGadgets(const Params& params,const State& state,
  PreparedIteration& prepared,const ModelPassResult& modelResult,
  CompletionServices& services,const Yield& yield);
} // namespace batbox::query
