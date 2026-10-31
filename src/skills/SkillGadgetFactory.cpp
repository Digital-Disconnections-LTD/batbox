#include "runtime/AsyncLaunch.hpp"
#include "skills/SkillGadgetFactory.hpp"
#include <future>
#include <memory>
#include <stdexcept>

namespace batbox::skills {
namespace {
template<class F>void need(const F& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Missing SkillTool binding: ")+name);
}
template<class T>std::future<T> ready(T value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]() mutable {
    return std::move(value);
  });
}
std::string skillName(const Json& input){
  const auto& value=value::property(input,"skill");
  return value.is_string()?value.get<std::string>():"undefined";
}
}
gadgets::Gadget makeSkillGadget(SkillGadgetFactoryServices services){
  need(services.getProjectRoot,"getProjectRoot");
  need(services.getAllCommandsForValidation,"getAllCommandsForValidation");
  need(services.getRules,"getRules");
  need(services.execution.getAllCommands,"getAllCommands");
  need(services.execution.lookup.findCommand,"findCommand");
  need(services.renderGadgetUseMessage,"renderToolUseMessage");
  need(services.renderGadgetResultMessage,"renderToolResultMessage");
  need(services.renderGadgetUseProgressMessage,"renderToolUseProgressMessage");
  need(services.renderGadgetUseRejectedMessage,"renderToolUseRejectedMessage");
  need(services.renderGadgetUseErrorMessage,"renderToolUseErrorMessage");
  auto state=std::make_shared<SkillGadgetFactoryServices>(std::move(services));
  gadgets::Gadget gadget;
  gadget.name="Skill";
  gadget.searchHint="invoke a slash-command skill";
  gadget.maxResultSizeChars=100000;
  gadget.inputJSONSchema=skillGadgetInputSchema();
  gadget.outputSchema=skillGadgetOutputSchema();
  gadget.parseInput=parseSkillGadgetInput;
  gadget.description=[](const Json& input,const Json&){
    return ready(std::string("Execute skill: ")+skillName(input));
  };
  gadget.poetry=[state](const gadgets::PoetryContext&){
    return ready(getSkillGadgetPoetry(state->getProjectRoot()));
  };
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    const auto& value=value::property(input,"skill");
    return value.is_null()?Json(""):value;
  };
  gadget.validateInput=[state](const Json& input,const Json& context){
    const auto skill=skillName(input);
    const auto trimmed=value::trim(skill);
    if(trimmed.empty()){
      gadgets::ValidationResult invalid;
      invalid.result=false;
      invalid.message="Invalid skill format: "+skill;
      invalid.errorCode=1;
      return ready(invalid);
    }
    const auto name=trimmed.starts_with('/')?value::substring(trimmed,1):trimmed;
    if(state->execution.remote.enabled){
      const auto remote=remote::validateRemoteSkill(name,state->execution.remote);
      if(remote)return ready(*remote);
    }
    const auto commands=state->getAllCommandsForValidation(context);
    return ready(validateSkillInput(skill,commands,state->execution.lookup));
  };
  gadget.checkPermissions=[state](const Json& input,const Json& context){
    const auto commands=state->getAllCommandsForValidation(context);
    const auto denyRules=state->getRules(context,"deny");
    const auto remote=[state](const std::string& name){
      return state->execution.remote.enabled&&
        remote::remoteSlugForCommand(name,state->execution.remote).has_value();
    };
    return ready(checkSkillPermissions(input,commands,denyRules,
      [state,context]{return state->getRules(context,"allow");},
      state->execution.lookup,"Skill",remote));
  };
  gadget.call=[state](const Json& input,const gadgets::GadgetCallContext& callContext,
    const std::function<std::future<Json>(const Json&)>& canUseGadget,
    const Json& parent,const std::function<void(const gadgets::GadgetProgress&)>& progress){
    if(!callContext.batContext)
      throw std::invalid_argument("Missing SkillTool binding: ToolCallContext.agentContext");
    return ready(executeSkill(input,*callContext.batContext,canUseGadget,parent,
      progress,state->execution,callContext.stopToken));
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=mapSkillGadgetResult;
  gadget.renderGadgetUseMessage=state->renderGadgetUseMessage;
  gadget.renderGadgetResultMessage=state->renderGadgetResultMessage;
  gadget.renderGadgetUseProgressMessage=state->renderGadgetUseProgressMessage;
  gadget.renderGadgetUseRejectedMessage=state->renderGadgetUseRejectedMessage;
  gadget.renderGadgetUseErrorMessage=state->renderGadgetUseErrorMessage;
  return gadgets::buildGadget(std::move(gadget));
}
}
