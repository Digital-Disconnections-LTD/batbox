#pragma once
#include "interactive/QueryExecution.hpp"
#include "interactive/submit/PoetrySubmit.hpp"

namespace batbox::interactive {
namespace context {class ContextFactory;}
// Capture per-turn application options at invocation. Mutable gadgets and MCP
// connections are read by getGadgetUseContext when execution begins.
using QueryExecutionParamsFactory=std::function<QueryExecutionParams()>;
void bindQueryTurnExecution(event_loop::Loop& owner,QueryTurnServices& turn,
  std::shared_ptr<QueryExecutionState> state,QueryExecutionParamsFactory params,
  std::shared_ptr<QueryExecutionServices> execution);
void bindSubmitQueryTurn(event_loop::Loop& owner,submit::SubmitServices& submit,
  std::shared_ptr<QueryTurnState> state,std::shared_ptr<QueryTurnServices> turn);
void bindQueryContextFactory(QueryExecutionServices& execution,
  std::shared_ptr<context::ContextFactory> factory);
} // namespace batbox::interactive
