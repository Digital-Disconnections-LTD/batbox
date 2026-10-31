#include "skills/process/PoetryCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::skills::process {
namespace {
template<class Fn>void need(const Fn& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Missing prompt command binding: ")+name);
}
std::string str(const Json& obj,const char* name){
  const auto& value=value::property(obj,name);
  return value.is_string()?value.get<std::string>():std::string{};
}
void copy(const Json& from,Json& to,const char* name){
  if(from.is_object()&&from.contains(name))to[name]=from.at(name);
}
Json createUser(const Json& content,const std::optional<std::string>& uuid,
  bool isMeta,PoetryCommandServices& services){
  Json args={{"content",content}};
  if(uuid)args["uuid"]=*uuid;
  if(isMeta)args["isMeta"]=true;
  return messages::createUserMessage(args,services.messageServices);
}
Json textBlocks(const Json& blocks,const std::string& separator){
  std::string joined;
  bool first=true;
  if(blocks.is_array())for(const auto& block:blocks){
    if(value::property(block,"type")!="text")continue;
    if(!first)joined+=separator;
    first=false;
    joined+=str(block,"text");
  }
  return joined;
}
Json concat(const Json& image,const Json& preceding,const Json& result){
  Json out=Json::array();
  for(const auto* array:{&image,&preceding,&result})
    if(array->is_array())for(const auto& block:*array)out.push_back(block);
  return out;
}
}
Json createAttachmentMessage(const Json& attachment,PoetryCommandServices& services){
  need(services.randomUUID,"randomUUID");
  need(services.nowISO,"nowISO");
  return {{"attachment",attachment},{"type","attachment"},
    {"uuid",services.randomUUID()},{"timestamp",services.nowISO()}};
}
Json processPoetrySlashCommand(const std::string& commandName,const std::string& args,
  const std::vector<Json>& commands,const bats::BatContext& context,
  PoetryCommandServices& services,const Json& imageContentBlocks){
  need(services.findCommand,"findCommand");
  const auto command=services.findCommand(commandName,commands);
  if(command.is_null())throw MalformedCommandError("Unknown command: "+commandName);
  if(value::property(command,"type")!="prompt"){
    const auto type=str(command,"type");
    throw std::runtime_error("Unexpected "+type+
      " command. Expected 'prompt' command. Use /"+commandName+
      " directly in the main conversation.");
  }
  return getMessagesForPoetrySlashCommand(command,args,context,services,
    Json::array(),imageContentBlocks);
}
Json getMessagesForPoetrySlashCommand(const Json& command,const std::string& args,
  const bats::BatContext& context,PoetryCommandServices& services,
  const Json& precedingInputBlocks,const Json& imageContentBlocks,
  const std::optional<std::string>& uuid){
  need(services.feature,"feature");
  const auto& contextData=context.query.data;
  if(services.feature("COORDINATOR_MODE")&&
     isEnvTruthy(services.coordinatorModeEnvironment)&&
     !value::truthy(value::property(contextData,"agentId"))){
    const auto metadata=formatCommandLoadingMetadata(command,args);
    const auto name=str(command,"name");
    std::vector<std::string> parts={"Skill \"/"+name+"\" is available for workers."};
    if(value::truthy(value::property(command,"description")))
      parts.push_back("Description: "+str(command,"description"));
    if(value::truthy(value::property(command,"whenToUse")))
      parts.push_back("When to use: "+str(command,"whenToUse"));
    const auto& rawAllowed=value::property(command,"allowedTools");
    if(rawAllowed.is_array()&&!rawAllowed.empty()){
      std::string allowed;
      bool first=true;
      for(const auto& gadget:rawAllowed){
        if(!first)allowed+=", ";
        first=false;
        if(gadget.is_string())allowed+=gadget.get<std::string>();
      }
      parts.push_back("This skill grants workers additional tool permissions: "+allowed);
    }
    parts.push_back("\nInstruct a worker to use this skill by including \"Use the /"+
      name+" skill\" in your Agent prompt. The worker has access to the Skill tool and will receive the skill's content and permissions when it invokes it.");
    std::string summary;
    for(const auto& part:parts){if(!summary.empty())summary+='\n';summary+=part;}
    Json out={{"messages",Json::array({
      createUser(metadata,uuid,false,services),
      createUser(Json::array({Json{{"type","text"},{"text",summary}}}),
        std::nullopt,true,services)})},
      {"shouldQuery",true},{"command",command}};
    copy(command,out,"model");copy(command,out,"effort");
    return out;
  }
  need(services.getPoetryForCommand,"getPromptForCommand");
  const auto result=services.getPoetryForCommand(command,args,context);
  need(services.isRestrictedToPluginOnly,"isRestrictedToPluginOnly");
  const bool restricted=services.isRestrictedToPluginOnly("hooks");
  const bool hooksAllowed=!restricted||
    isSourceAdminTrusted(value::property(command,"source"));
  if(value::truthy(value::property(command,"hooks"))&&hooksAllowed){
    need(services.getFlightId,"getSessionId");
    need(services.registerSkillHooks,"registerSkillHooks");
    const auto flightId=services.getFlightId();
    const auto& root=value::property(command,"skillRoot");
    services.registerSkillHooks(context,flightId,value::property(command,"hooks"),
      str(command,"name"),root.is_string()
        ?std::optional<std::string>(root.get<std::string>()):std::nullopt);
  }
  const auto name=str(command,"name");
  const auto source=str(command,"source");
  const auto path=!source.empty()?source+":"+name:name;
  const auto content=textBlocks(result,"\n\n").get<std::string>();
  need(services.currentBatId,"currentAgentId");
  need(services.addInvokedSkill,"addInvokedSkill");
  services.addInvokedSkill(name,path,content,services.currentBatId());
  const auto metadata=formatCommandLoadingMetadata(command,args);
  const auto& rawAllowed=value::property(command,"allowedTools");
  const auto allowed=parseGadgetListFromCLI(rawAllowed.is_null()?Json::array():rawAllowed);
  const Json main=(imageContentBlocks.is_array()&&!imageContentBlocks.empty())||
    (precedingInputBlocks.is_array()&&!precedingInputBlocks.empty())
    ?concat(imageContentBlocks,precedingInputBlocks,result):result;
  need(services.getAttachmentMessages,"getAttachmentMessages");
  const auto attachments=services.getAttachmentMessages(
    textBlocks(result," ").get<std::string>(),context,Json::array(),
    value::property(contextData,"messages"),"repl_main_thread",true);
  Json outputMessages=Json::array({createUser(metadata,uuid,false,services),
    createUser(main,std::nullopt,true,services)});
  if(!attachments.is_array())throw std::runtime_error("Attachment generator did not yield an array");
  for(const auto& attachment:attachments)outputMessages.push_back(attachment);
  Json permission={{"type","command_permissions"},{"allowedTools",allowed}};
  copy(command,permission,"model");
  outputMessages.push_back(createAttachmentMessage(permission,services));
  Json out={{"messages",outputMessages},{"shouldQuery",true},
    {"allowedTools",allowed},{"command",command}};
  copy(command,out,"model");copy(command,out,"effort");
  return out;
}
}
