#include "interactive/QueryTurnInternal.hpp"
#include "messages/TextContent.hpp"

namespace batbox::interactive {
event_loop::Deferred<event_loop::Unit> runQueryTurn(event_loop::Loop& owner,
    std::shared_ptr<QueryTurnState> state,TurnRequest request,
    std::shared_ptr<QueryTurnServices> services){
  if(!owner.isOwnerThread())throw std::logic_error("Query turn requires owner loop");
  if(!state||!services||!request.newMessages||!request.abort)
    throw std::invalid_argument("Query turn state, messages and abort controller required");
  auto run=std::make_shared<query_turn::Run>(owner,std::move(state),
    std::move(request),std::move(services));
  run->start();return run->result;
}
}
namespace batbox::interactive::query_turn {
void Run::start(){
  try{
    auto& s=*services;
    need(s.isBatSwarmsEnabled,"isAgentSwarmsEnabled");
    if(s.isBatSwarmsEnabled()){
      need(s.getTeamName,"getTeamName");need(s.getBatName,"getAgentName");
      const auto team=s.getTeamName(),bat=s.getBatName();
      if(team&&!team->empty()&&bat&&!bat->empty()){
        need(s.setMemberActive,"setMemberActive");s.setMemberActive(*team,*bat,true);
      }
    }
    generation=state->guard.tryStart();
    if(!generation){
      std::vector<std::string> queued;
      for(const auto& message:*request.newMessages){
        if(value::property(*message,"type")!="user"||
            value::truthy(value::property(*message,"isMeta")))continue;
        const auto text=messages::getContentText(value::property(
          value::property(*message,"message"),"content"));
        if(text)queued.push_back(*text);
      }
      for(const auto& text:queued){
        need(s.enqueue,"enqueue");s.enqueue(Json{{"value",text},{"mode","prompt"}});
      }
      result.resolve(event_loop::Unit{});return;
    }
  }catch(...){result.reject(std::current_exception());return;}
  try{prepare();}catch(...){finalize(std::current_exception());}
}
void Run::prepare(){
  auto& s=*services;
  need(s.resetTimingRefs,"resetTimingRefs");s.resetTimingRefs();
  need(s.setMessages,"setMessages");
  const auto additions=request.newMessages;
  s.setMessages([additions](const TurnMessagesRef& old){
    auto messages=std::make_shared<TurnMessages>(*old);
    messages->insert(messages->end(),additions->begin(),additions->end());return messages;
  });
  need(s.setResponseLengthRef,"setResponseLengthRef");s.setResponseLengthRef(0);
  need(s.feature,"feature");
  if(s.feature("TOKEN_BUDGET")){
    std::optional<double> budget;
    if(request.input&&!request.input->empty()){
      need(s.parseTokenBudget,"parseTokenBudget");budget=s.parseTokenBudget(*request.input);
    }
    if(!budget){need(s.getCurrentTurnTokenBudget,"getCurrentTurnTokenBudget");
      budget=s.getCurrentTurnTokenBudget();}
    need(s.snapshotOutputTokensForTurn,"snapshotOutputTokensForTurn");
    s.snapshotOutputTokensForTurn(budget);
  }
  need(s.clearApiMetrics,"clearApiMetrics");s.clearApiMetrics();
  need(s.clearStreamingGadgetUses,"clearStreamingToolUses");s.clearStreamingGadgetUses();
  need(s.clearStreamingText,"clearStreamingText");s.clearStreamingText();
  need(s.getMessages,"getMessages");latestMessages=s.getMessages();
  if(request.input&&!request.input->empty()){
    need(s.mrOnBeforeQuery,"mrOnBeforeQuery");
    auto self=shared_from_this();
    await(s.mrOnBeforeQuery(*request.input,latestMessages,request.newMessages->size()),
      [self](const event_loop::Unit&){self->beforeQuery();});
  }else beforeQuery();
}
void Run::beforeQuery(){
  if(request.onBeforeQuery&&request.input&&!request.input->empty()){
    auto self=shared_from_this();
    await(request.onBeforeQuery(*request.input,latestMessages),[self](bool proceed){
      if(proceed)self->invokeQuery();else self->finalize();
    });
  }else invokeQuery();
}
void Run::invokeQuery(){
  need(services->onQueryImpl,"onQueryImpl");auto self=shared_from_this();
  await(services->onQueryImpl(latestMessages,request),
    [self](const event_loop::Unit&){self->finalize();});
}
} // namespace batbox::interactive::query_turn
