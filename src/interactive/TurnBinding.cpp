#include "interactive/TurnBinding.hpp"
#include "interactive/context/ContextFactory.hpp"
#include <stdexcept>

namespace batbox::interactive {
void bindQueryContextFactory(QueryExecutionServices& execution,
    std::shared_ptr<context::ContextFactory> factory){
  if(!factory)throw std::invalid_argument("Interactive context factory required");
  execution.getGadgetUseContext=[factory=std::move(factory)](
      const TurnMessagesRef& messages,const TurnRequest& request){
    return factory->makeQueryContext(messages,request);
  };
}
void bindQueryTurnExecution(event_loop::Loop& owner,QueryTurnServices& turn,
    std::shared_ptr<QueryExecutionState> state,QueryExecutionParamsFactory factory,
    std::shared_ptr<QueryExecutionServices> execution){
  if(!state||!factory||!execution)
    throw std::invalid_argument("Interactive query execution bindings required");
  turn.onQueryImpl=[&owner,state=std::move(state),factory=std::move(factory),
      execution=std::move(execution)](const TurnMessagesRef& messages,const TurnRequest& request){
    auto params=factory();params.messages=messages;params.request=request;
    return executeQueryTurn(owner,state,std::move(params),execution);
  };
}
void bindSubmitQueryTurn(event_loop::Loop& owner,submit::SubmitServices& submit,
    std::shared_ptr<QueryTurnState> state,std::shared_ptr<QueryTurnServices> turn){
  if(!state||!turn)throw std::invalid_argument("Interactive query turn bindings required");
  if(&submit.loop!=&owner||&submit.guard!=&state->guard)
    throw std::invalid_argument("Submit and query turn must share one owner and guard");
  submit.onQuery=[&owner,state=std::move(state),turn=std::move(turn)](TurnRequest request){
    return runQueryTurn(owner,state,std::move(request),turn);
  };
}
} // namespace batbox::interactive
