#include "application_skills/Provider.hpp"
#include "application/Async.hpp"
#include "application_hooks/Provider.hpp"
#include "application_media/AttachmentAggregate.hpp"
#include "application_flights/Flight.hpp"
#include "bats/ForkedBat.hpp"
#include "bats/RunBat.hpp"
#include "skills/process/PoetryCommand.hpp"
#include "user_input/CommandMessages.hpp"
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
  services.getPoetryForCommand=[this](const Json& command,
      const std::string& args,const bats::BatContext& bat){
    return poetryFor(command,args,bat);};
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
void Provider::prepareForkedRun(const Json& descriptor,const std::string& args,
    const bats::BatContext& context,
    const std::function<std::future<Json>(const Json&)>& canUseGadget,
    bats::RunBatParams& params){
  bats::CommandServices services;
  services.getPoetryForCommand=[this](const Json& command,const std::string& text,
      const execution::Context& query){
    bats::BatContext bat;bat.query=query;
    return poetryFor(command,text,bat);};
  services.parseGadgetListFromCLI=skills::process::parseGadgetListFromCLI;
  services.createUserMessage=[messages=inputs_.messages](const Json& value){
    return messages::createUserMessage(value,*messages);};
  const auto prepared=bats::prepareForkedCommandContext(descriptor,args,context.query,
    context.getAppState,services);
  params.batDefinition=prepared.baseBat;
  if(descriptor.is_object()&&descriptor.contains("effort"))
    params.batDefinition["effort"]=descriptor["effort"];
  params.poetryMessages=prepared.poetryMessages;
  params.gadgetUseContext=context;
  params.gadgetUseContext.getAppState=prepared.modifiedGetAppState;
  params.canUseGadget=canUseGadget;
  params.querySource="agent:custom";
  const auto& model=value::property(descriptor,"model");
  if(model.is_string())params.model=model.get<std::string>();
  params.availableGadgets=context.query.gadgets;
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
    if(value::property(item,"context")=="fork")
      entry.runForked=[weak,item,display=skills::userFacingName(command)](
          const std::string& args,const Json& preceding){
        auto self=weak.lock();
        if(!self)throw std::runtime_error("Skill catalog has closed");
        if(!self->launchSkillBat_)throw std::runtime_error("Skill Bats are unavailable");
        const auto title="/"+display;
        auto started=self->launchSkillBat_(title,[weak,item,args](
            const bats::BatContext& context,
            const std::function<std::future<Json>(const Json&)>& canUseGadget,
            bats::RunBatParams& params){
          auto self=weak.lock();
          if(!self)throw std::runtime_error("Skill catalog has closed");
          self->prepareForkedRun(item,args,context,canUseGadget,params);
        });
        event_loop::Deferred<user_input::InputResult> output(self->inputs_.owner);
        started.then([output,messages=self->inputs_.messages,display,title,args,preceding](
            const event_loop::Unit*,const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          try{
            user_input::InputResult result;
            const auto text="Started "+title+" as a Bat. Its result comes back "
              "to this conversation when it finishes.";
            result.messages.push_back(messages::createUserMessage(Json{{"content",
              messages::prepareUserContent(Json{{"inputString",
                user_input::formatCommandInputTags(display,args)},
                {"precedingInputBlocks",preceding}})}},*messages));
            result.messages.push_back(user_input::createCommandInputMessage(
              "<local-command-stdout>"+text+"</local-command-stdout>",*messages));
            result.resultText=text;
            output.resolve(std::move(result));
          }catch(...){output.reject(std::current_exception());}
        });
        return output;
      };
    result.push_back(std::move(entry));
  }
  return result;
}
}
