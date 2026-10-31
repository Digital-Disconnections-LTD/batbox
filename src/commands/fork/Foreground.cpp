#include "commands/fork/Internal.hpp"
#include "commands/Roster.hpp"
#include "utils/tokens.hpp"
#include <stdexcept>

namespace batbox::commands::fork::internal {
namespace {
struct ForegroundState {
  Prepared prepared;
  Json preceding,progress=Json::array();
  std::vector<query::MessageRef> messages;
  std::string parentGadgetUseID;
  std::size_t gadgetUseCounter=0;
  std::exception_ptr messageError;
  Services& services;
  event_loop::Deferred<user_input::InputResult> output;
  ForegroundState(Prepared prepared,Json preceding,Services& services):
    prepared(std::move(prepared)),preceding(std::move(preceding)),
    parentGadgetUseID("forked-command-"+value::property(this->prepared.command,"name").get<std::string>()),
    services(services),output(services.loop){}
};
void show(const std::shared_ptr<ForegroundState>& state){
  require(static_cast<bool>(state->services.showProgress),"showProgress");
  state->services.showProgress(state->progress);
}
void addProgress(const std::shared_ptr<ForegroundState>& state,const Json& message){
  require(static_cast<bool>(state->services.nowISO),"nowISO");
  require(static_cast<bool>(state->services.randomUUID),"randomUUID");
  const auto id=state->parentGadgetUseID+"-"+
    std::to_string(++state->gadgetUseCounter);
  state->progress.push_back(Json{{"type","progress"},
    {"data",{{"message",message},{"type","agent_progress"},
      {"prompt",state->prepared.skillContent},{"agentId",state->prepared.batId}}},
    {"parentToolUseID",state->parentGadgetUseID},{"toolUseID",id},
    {"timestamp",state->services.nowISO()},
    {"uuid",state->services.randomUUID()}});
  show(state);
}
void receive(const std::shared_ptr<ForegroundState>& state,
  const query::MessageRef& reference){
  if(!reference)throw std::invalid_argument("Agent yielded null message");
  state->messages.push_back(reference);
  const Json& message=*reference;
  const auto& type=value::property(message,"type");
  if(type!="assistant"&&type!="user")return;
  if(type=="assistant"){
    const double length=::batbox::getAssistantMessageContentLength(message);
    if(length>0){
      require(static_cast<bool>(state->services.context.updateResponseLength),
        "setResponseLength");
      state->services.context.updateResponseLength([length](const Json& current){
        return value::numberOr(current)+length;
      });
    }
  }
  const Json normalized=messages::normalizeMessages(Json::array({message}),
    state->services.messages);
  if(!normalized.is_array()||normalized.empty())return;
  if(type=="assistant"&&value::property(normalized[0],"type")=="assistant")
    addProgress(state,message);
  else if(type=="user"&&value::property(normalized[0],"type")=="user")
    addProgress(state,normalized[0]);
}
user_input::InputResult result(const std::shared_ptr<ForegroundState>& state){
  auto& services=state->services;
  const auto& prepared=state->prepared;
  Json batMessages=Json::array();
  for(const auto& message:state->messages)batMessages.push_back(*message);
  const auto text=bats::extractResultText(batMessages,"Command completed");
  if(services.logDebug)services.logDebug("Forked slash command /"+
    value::property(prepared.command,"name").get<std::string>()+
    " completed with agent "+prepared.batId);
  const auto commandInput=value::trim("/"+prepared.commandName+" "+prepared.args);
  Json content=messages::prepareUserContent(Json{{"inputString",commandInput},
    {"precedingInputBlocks",state->preceding}});
  user_input::InputResult output;
  output.messages.push_back(messages::createUserMessage(Json{{"content",content}},
    services.messages));
  output.messages.push_back(messages::createUserMessage(Json{{"content",
    "<local-command-stdout>\n"+text+"\n</local-command-stdout>"}},services.messages));
  output.resultText=text;
  return output;
}
} // namespace
event_loop::Deferred<user_input::InputResult> runForeground(
  Prepared prepared,Json preceding,Services& services){
  auto state=std::make_shared<ForegroundState>(std::move(prepared),
    std::move(preceding),services);
  try{
    require(static_cast<bool>(services.runBat),"runAgent");
    require(static_cast<bool>(services.clearProgress),"clearProgress");
    show(state); // Initializing state, before the first bat message.
    auto params=paramsFor(state->prepared,services,false,services.context.query.gadgets);
    auto pending=services.runBat(std::move(params),
      [state](const query::MessageRef& message){
      if(state->messageError)return;
      try{receive(state,message);}
      catch(...){state->messageError=std::current_exception();}
    });
    pending.then([state](const event_loop::Unit*,const std::exception_ptr& error){
      try{
        state->services.clearProgress();
        if(error)std::rethrow_exception(error);
        if(state->messageError)std::rethrow_exception(state->messageError);
        state->output.resolve(result(state));
      }catch(...){state->output.reject(std::current_exception());}
    });
  }catch(...){
    try{if(services.clearProgress)services.clearProgress();}catch(...){}
    state->output.reject(std::current_exception());
  }
  return state->output;
}
} // namespace batbox::commands::fork::internal
