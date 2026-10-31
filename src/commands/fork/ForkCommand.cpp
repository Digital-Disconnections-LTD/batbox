#include "commands/fork/Internal.hpp"
#include "commands/Roster.hpp"
#include <stdexcept>

namespace batbox::commands::fork {
namespace internal {
void require(bool present,const char* name){
  if(!present)throw std::invalid_argument(std::string("Forked command requires ")+name);
}
std::string errorMessage(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& e){return e.what();}
  catch(...){return "Unknown error";}
}
Prepared prepare(const Json& command,const std::string& args,Services& services){
  require(static_cast<bool>(services.createBatId),"createAgentId");
  Prepared output;
  output.command=command;
  output.args=args;
  output.batId=services.createBatId();
  output.commandName=commands::getCommandName(command);
  auto imports=services.prepare;
  imports.createUserMessage=[&services](const Json& value){
    return messages::createUserMessage(value,services.messages);
  };
  const auto forked=bats::prepareForkedCommandContext(command,args,
    services.context.query,services.context.getAppState,imports);
  output.skillContent=forked.skillContent;
  output.modifiedGetAppState=forked.modifiedGetAppState;
  output.poetryMessages=forked.poetryMessages;
  output.batDefinition=forked.baseBat;
  if(command.is_object()&&command.contains("effort"))
    output.batDefinition["effort"]=command["effort"];
  if(services.logDebug){
    const auto& batType=value::property(output.batDefinition,"agentType");
    services.logDebug("Executing forked slash command /"+
      output.commandName+" with agent "+
      (batType.is_string()?batType.get<std::string>():value::stringify(batType)));
  }
  return output;
}
bats::RunBatParams paramsFor(const Prepared& prepared,Services& services,
  bool async,const gadgets::Gadgets& availableGadgets){
  bats::RunBatParams params;
  params.batDefinition=prepared.batDefinition;
  params.poetryMessages=prepared.poetryMessages;
  params.gadgetUseContext=services.context;
  params.gadgetUseContext.getAppState=prepared.modifiedGetAppState;
  params.canUseGadget=services.canUseGadget;
  params.isAsync=async;
  params.querySource="agent:custom";
  const auto& model=value::property(prepared.command,"model");
  if(model.is_string())params.model=model.get<std::string>();
  params.availableGadgets=availableGadgets;
  if(async){
    params.override={{"agentId",prepared.batId}};
    params.overrideAbortOwner=std::make_shared<std::stop_source>();
    params.gadgetUseContext.abortOwner=params.overrideAbortOwner;
  }
  return params;
}
} // namespace internal

event_loop::Deferred<user_input::InputResult> executeForkedSlashCommand(
  const Json& command,const std::string& args,const Json& preceding,
  Services& services){
  event_loop::Deferred<user_input::InputResult> output(services.loop);
  try{
    auto prepared=internal::prepare(command,args,services);
    internal::require(static_cast<bool>(services.feature),"feature");
    internal::require(static_cast<bool>(services.context.getAppState),"getAppState");
    if(services.feature("KAIROS")&&value::truthy(value::property(
      services.context.getAppState(),"kairosEnabled")))
      return internal::runBackground(std::move(prepared),services);
    return internal::runForeground(std::move(prepared),preceding,services);
  }catch(...){output.reject(std::current_exception());return output;}
}
user_input::CommandEntry bindForkedCommand(Json descriptor,
  std::shared_ptr<Services> services){
  if(!services)throw std::invalid_argument("Forked command services required");
  user_input::CommandEntry entry;
  entry.descriptor=std::move(descriptor);
  entry.runForked=[command=entry.descriptor,services=std::move(services)](
    const std::string& args,const Json& preceding){
    return executeForkedSlashCommand(command,args,preceding,*services);
  };
  return entry;
}
} // namespace batbox::commands::fork
