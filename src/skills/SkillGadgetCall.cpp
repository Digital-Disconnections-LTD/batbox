#include "skills/SkillGadgetExecution.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::skills {
namespace {
template<class F>void need(const F& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing SkillTool binding: ")+name);
}
std::string text(const Json& object,const char* key){
  const auto& value=value::property(object,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
std::vector<Json> asVector(const Json& values){
  std::vector<Json> result;
  if(values.is_array())for(const auto& value:values)result.push_back(value);
  return result;
}
std::vector<std::string> asStrings(const Json& values){
  std::vector<std::string> result;
  if(values.is_array())for(const auto& value:values)
    if(value.is_string())result.push_back(value.get<std::string>());
  return result;
}
Json find(const std::string& name,const std::vector<Json>& commands,
  const SkillGadgetLookup& lookup){
  need(lookup.findCommand,"findCommand");
  return lookup.findCommand(name,commands);
}
std::string normalizedName(const Json& input){
  const auto trimmed=value::trim(text(input,"skill"));
  return trimmed.starts_with('/')?value::substring(trimmed,1):trimmed;
}
Json messagesToJson(const std::vector<query::MessageRef>& refs){
  Json output=Json::array();for(const auto& item:refs)output.push_back(*item);
  return output;
}
}
gadgets::GadgetResult executeSkill(const Json& input,
  const bats::BatContext& context,
  const std::function<std::future<Json>(const Json&)>& canUseGadget,
  const Json& parentMessage,const std::function<void(const gadgets::GadgetProgress&)>& onProgress,
  SkillExecutionServices& services,std::stop_token stopToken){
  const auto commandName=normalizedName(input);
  if(services.remote.enabled){
    const auto slug=remote::remoteSlugForCommand(commandName,services.remote);
    if(slug){
      auto remoteServices=services.remote;
      remoteServices.recordSkillUsage=services.recordSkillUsage;
      return remote::executeRemoteSkill(*slug,commandName,parentMessage,
        remoteServices,stopToken);
    }
  }
  need(services.getAllCommands,"getAllCommands");
  const auto commands=services.getAllCommands(context);
  const auto command=find(commandName,commands,services.lookup);
  need(services.recordSkillUsage,"recordSkillUsage");
  services.recordSkillUsage(commandName);
  if(command.is_null())throw std::runtime_error("Unknown skill: "+commandName);
  const auto args=text(input,"args");
  if(value::property(command,"type")=="prompt"&&
     value::property(command,"context")=="fork"){
    need(services.createBatId,"createAgentId");
    need(services.runBat,"runAgent");
    need(services.clearInvokedSkillsForBat,"clearInvokedSkillsForAgent");
    const auto batId=services.createBatId();
    auto commandServices=services.forkedCommand;
    auto prepared=bats::prepareForkedCommandContext(command,args,context.query,
      context.getAppState,commandServices);
    auto definition=prepared.baseBat;
    const auto& effort=value::property(command,"effort");
    if(command.is_object()&&command.contains("effort"))definition["effort"]=effort;
    bats::RunBatParams params;
    params.batDefinition=definition;
    params.poetryMessages=prepared.poetryMessages;
    params.gadgetUseContext=context;
    params.gadgetUseContext.getAppState=prepared.modifiedGetAppState;
    params.canUseGadget=canUseGadget;
    params.isAsync=false;
    params.querySource="agent:custom";
    const auto& model=value::property(command,"model");
    if(model.is_string())params.model=model.get<std::string>();
    params.availableGadgets=context.query.gadgets;
    params.override={{"agentId",batId}};
    std::vector<query::MessageRef> messages;
    const auto messageId=text(value::property(parentMessage,"message"),"id");
    const query::Yield yield=query::Yield::withReferences(
      [&](const query::MessageRef& message){
        messages.push_back(message);
        const auto& type=value::property(*message,"type");
        if((type=="assistant"||type=="user")&&onProgress){
          need(services.normalizeMessages,"normalizeMessages");
          const auto normalized=services.normalizeMessages(Json::array({*message}));
          if(normalized.is_array())for(const auto& item:normalized){
            const auto& content=value::property(value::property(item,"message"),"content");
            bool hasGadgetContent=false;
            if(content.is_array())for(const auto& block:content)
              if(value::property(block,"type")=="tool_use"||
                 value::property(block,"type")=="tool_result"){
                hasGadgetContent=true;break;
              }
            if(hasGadgetContent)onProgress({"skill_"+messageId,
              {{"message",item},{"type","skill_progress"},
               {"prompt",prepared.skillContent},{"agentId",batId}}});
          }
        }
      });
    bool clearing=false;
    try{
      services.runBat(params,yield);
      const auto resultText=bats::extractResultText(messagesToJson(messages),
        "Skill execution completed");
      gadgets::GadgetResult output;
      output.data={{"success",true},{"commandName",commandName},
        {"status","forked"},{"agentId",batId},{"result",resultText}};
      clearing=true;
      services.clearInvokedSkillsForBat(batId);
      return output;
    }catch(...){
      if(!clearing)services.clearInvokedSkillsForBat(batId);
      throw;
    }
  }
  need(services.processPoetrySlashCommand,"processPromptSlashCommand");
  const auto processed=services.processPoetrySlashCommand(commandName,args,commands,context);
  if(!value::truthy(value::property(processed,"shouldQuery")))
    throw std::runtime_error("Command processing failed");
  const auto& rawAllowed=value::property(processed,"allowedTools");
  const auto allowedGadgets=asStrings(rawAllowed);
  const auto& model=value::property(processed,"model");
  const Json effort=value::property(command,"type")=="prompt"
    ?value::property(command,"effort"):Json(nullptr);
  const auto filtered=filterSkillProcessedMessages(asVector(value::property(
    processed,"messages")),services.commandMessageTag);
  const auto id=getSkillGadgetUseIDFromParentMessage(parentMessage);
  const auto tagged=tagSkillMessagesWithGadgetUseID(filtered,id);
  gadgets::GadgetResult output;
  output.data={{"success",true},{"commandName",commandName}};
  if(!allowedGadgets.empty())output.data["allowedTools"]=allowedGadgets;
  if(processed.is_object()&&processed.contains("model"))output.data["model"]=model;
  output.newMessages=tagged;
  SkillContextModifier modifier;
  modifier.allowedGadgets=allowedGadgets;
  if(model.is_string())modifier.model=model.get<std::string>();
  modifier.effort=effort;
  modifier.effortPresent=value::property(command,"type")=="prompt"&&
    command.is_object()&&command.contains("effort");
  modifier.has1mContext=services.has1mContext;
  modifier.parseUserSpecifiedModel=services.parseUserSpecifiedModel;
  modifier.modelSupports1M=services.modelSupports1M;
  output.contextModifier=[modifier](const Json& ctx){
    return applySkillOptionsModifier(ctx,modifier);
  };
  output.appStateModifier=[modifier](const Json& state){
    return applySkillAppStateModifier(state,modifier);
  };
  return output;
}
}
