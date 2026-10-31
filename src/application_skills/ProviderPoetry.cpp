#include "application_skills/Provider.hpp"
#include "application/Async.hpp"
#include "application_hooks/Provider.hpp"
#include "application_media/AttachmentAggregate.hpp"
#include "application_flights/Flight.hpp"
#include "bats/ForkedBat.hpp"
#include "runtime/Platform.hpp"
#include <stdexcept>

namespace batbox::application_skills {
namespace {
std::string text(const Json& value){
  return value.is_string()?value.get<std::string>():std::string{};
}
}
Json Provider::poetryFor(const Json& descriptor,const std::string& args,
    const bats::BatContext& context){
  const auto name=text(value::property(descriptor,"name"));
  const auto dispatcher=inputs_.owner.dispatcher();
  auto command=application::onOwnerAndWait<skills::SkillCommand>(dispatcher,
    [this,name]{
      if(!commandIndex_.contains(name))
        for(const auto& item:loadedSkills())commandIndex_[item.name]=item;
      const auto found=commandIndex_.find(name);
      if(found==commandIndex_.end())throw std::runtime_error(
        "Skill is no longer available: "+name);
      return found->second;
    });
  auto services=services_.skill;
  services.executeShellCommandsInPoetry=[this,context](
      const std::string& text,const std::string& slashCommandName,
      const std::optional<std::string>& shell,
      const std::vector<std::string>& allowedGadgets){
    return executePoetryShell(text,slashCommandName,shell,allowedGadgets,context);
  };
  return skills::getPoetryForCommand(command,args,services);
}
skills::process::PoetryCommandServices Provider::poetryServices(
    const bats::BatContext& context){
  skills::process::PoetryCommandServices services;
  services.messageServices=*inputs_.messages;
  services.findCommand=[this](const std::string& name,
      const std::vector<Json>& commands){return findCommand(name,commands);};
  services.getPoetryForCommand=[this](const Json& command,
      const std::string& args,const bats::BatContext& bat){
    return poetryFor(command,args,bat);};
  services.feature=[](const std::string&){return false;};
  services.isRestrictedToPluginOnly=[](const std::string&){return false;};
  const auto dispatcher=inputs_.owner.dispatcher();
  services.getFlightId=[dispatcher,flight=inputs_.flight]{
    return application::onOwnerAndWait<std::string>(dispatcher,
      [flight]{return flight->id();});};
  services.registerSkillHooks=[this,dispatcher](const bats::BatContext&,
      const std::string& flightId,const Json& hooks,const std::string& name,
      const std::optional<std::string>& root){
    if(!hooks.is_object()||hooks.empty())return;
    application::onOwnerAndWait<void>(dispatcher,[this,flightId,hooks,name,root]{
      if(!hooks_)throw std::runtime_error("Skill hook provider is unavailable");
      hooks_->registerFrontmatterHooks(flightId,hooks,name,false,root);});};
  const auto batId=value::property(context.query.data,"agentId");
  services.currentBatId=[batId]()->std::optional<std::string>{
    if(batId.is_string())return batId.get<std::string>();
    return {};};
  services.addInvokedSkill=[dispatcher,invoked=inputs_.invoked](
      const std::string& name,const std::string& path,
      const std::string& content,const std::optional<std::string>& batId){
    application::onOwnerAndWait<void>(dispatcher,
      [invoked,name,path,content,batId]{
        bootstrap::addInvokedSkill(*invoked,name,path,content,batId,
          bootstrap::indigenousClockMillis);});};
  services.getAttachmentMessages=[this,dispatcher](const std::string& content,
      const bats::BatContext&,const Json&,const Json&,
      const std::string& source,bool){
    return application::awaitOnOwner<Json>(dispatcher,
      [this,content,source]{
        if(!attachments_)throw std::logic_error(
          "Skill attachment provider is unavailable");
        return attachments_->collectPoetry(content,Json(),Json::array(),
          inputs_.currentMessages(),source);
      });};
  services.randomUUID=inputs_.messages->randomUUID;
  services.nowISO=inputs_.messages->nowISO;
  return services;
}
Json Provider::processPoetry(const std::string& name,const std::string& args,
    const bats::BatContext& context,const Json& preceding,
    const Json& images,const std::optional<std::string>& uuid){
  const auto all=application::onOwnerAndWait<std::vector<Json>>(
    inputs_.owner.dispatcher(),[this]{
      const auto local=allCommands();
      std::vector<Json> mcp;
      if(inputs_.mcpCommands){
        const auto rows=inputs_.mcpCommands();
        if(rows.is_array())for(const auto& row:rows)mcp.push_back(row);
      }
      return skills::getAllSkillCommands(local,mcp);});
  const auto command=findCommand(name,all);
  if(command.is_null())throw skills::process::MalformedCommandError(
    "Unknown command: "+name);
  if(value::property(command,"loadedFrom")=="mcp"){
    if(!mcpPoetry_)throw std::runtime_error("MCP Skill prompt runner is unavailable");
    return mcpPoetry_(command,args,context);
  }
  auto services=poetryServices(context);
  return skills::process::getMessagesForPoetrySlashCommand(command,args,
    context,services,preceding,images,uuid);
}
std::vector<user_input::CommandEntry> Provider::slashCommands(){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Skill slash catalog requires owner loop");
  std::vector<user_input::CommandEntry> result;
  for(const auto& command:loadedSkills()){
    commandIndex_[command.name]=command;
    if(!command.userInvocable)continue;
    const auto item=descriptor(command);
    user_input::CommandEntry entry;
    entry.descriptor=item;
    entry.userFacingName=[name=skills::userFacingName(command)]{return name;};
    auto weak=weak_from_this();
    entry.expandPoetry=[weak,name=command.name](const std::string& args,
        const Json& preceding,const Json& images,
        const std::optional<std::string>& uuid){
      auto self=weak.lock();
      if(!self)throw std::runtime_error("Skill catalog has closed");
      auto bat=self->inputs_.currentBat();
      bats::BatContext context=bat?*bat:bats::BatContext{};
      if(!context.query.data.is_object())context.query.data=Json::object();
      context.query.data["messages"]=self->inputs_.currentMessages();
      return application::runInWorker<user_input::InputResult>(self->inputs_.owner,
        [self,name,args,preceding,images,uuid,context=std::move(context)]{
          const auto processed=self->processPoetry(name,args,context,
            preceding,images,uuid);
          user_input::InputResult output;
          output.messages=value::property(processed,"messages");
          output.shouldQuery=value::truthy(value::property(processed,"shouldQuery"));
          const auto& allowed=value::property(processed,"allowedTools");
          if(!allowed.is_null())output.allowedGadgets=allowed;
          const auto& model=value::property(processed,"model");
          if(model.is_string())output.model=model.get<std::string>();
          const auto& effort=value::property(processed,"effort");
          if(effort.is_string())output.effort=effort.get<std::string>();
          return output;
        });
    };
    result.push_back(std::move(entry));
  }
  return result;
}
}
