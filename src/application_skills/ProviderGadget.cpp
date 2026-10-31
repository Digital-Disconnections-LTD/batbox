#include "application_skills/Provider.hpp"
#include "application/Async.hpp"
#include "bats/ForkedBat.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "settings/PermissionRules.hpp"
#include "skills/SkillGadgetFactory.hpp"
#include <stdexcept>

namespace batbox::application_skills {
namespace {
std::string string(const Json& value){
  return value.is_string()?value.get<std::string>():std::string{};
}
std::vector<Json> vector(const Json& value){
  std::vector<Json> rows;
  if(value.is_array())for(const auto& item:value)rows.push_back(item);
  return rows;
}
std::string visible(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return {};
  return value.dump();
}
}
skills::RuleEntries Provider::rules(const Json& context,
    const std::string& behavior)const{
  const auto& nested=value::property(context,"toolPermissionContext");
  const auto& permissions=nested.is_object()?nested:context;
  const char* key=behavior=="deny"?"alwaysDenyRules":"alwaysAllowRules";
  const auto& raw=value::property(permissions,key);
  skills::RuleEntries result;
  auto append=[&result](const Json& values){
    if(!values.is_array())return;
    for(const auto& entry:values){
      if(!entry.is_string())continue;
      const auto rule=entry.get<std::string>();
      const auto parsed=settings::permissionRuleValueFromString(rule,{});
      if(parsed.gadgetName!="Skill"||!parsed.ruleContent)continue;
      result.emplace_back(*parsed.ruleContent,
        Json{{"source","settings"},{"rule",rule}});
    }
  };
  append(raw);
  if(raw.is_object())for(auto it=raw.begin();it!=raw.end();++it)
    append(it.value());
  return result;
}
gadgets::Gadget Provider::makeGadget(){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Skill tool construction requires owner loop");
  const auto dispatcher=inputs_.owner.dispatcher();
  auto weak=weak_from_this();
  skills::SkillGadgetFactoryServices gadget;
  gadget.getProjectRoot=[root=inputs_.projectRoot]{return root.string();};
  const auto all=[weak,dispatcher]{
    return application::onOwnerAndWait<std::vector<Json>>(dispatcher,[weak]{
      if(auto self=weak.lock()){
        const auto local=self->allCommands();
        const auto mcp=self->inputs_.mcpCommands
          ?vector(self->inputs_.mcpCommands()):std::vector<Json>{};
        return skills::getAllSkillCommands(local,mcp);
      }
      throw std::runtime_error("Skill catalog has closed");
    });
  };
  gadget.getAllCommandsForValidation=[all](const Json&){return all();};
  gadget.getRules=[weak](const Json& context,const std::string& behavior){
    if(auto self=weak.lock())return self->rules(context,behavior);
    throw std::runtime_error("Skill permission owner has closed");};
  gadget.execution.getAllCommands=[all](const bats::BatContext&){return all();};
  gadget.execution.lookup.findCommand=[weak](const std::string& name,
      const std::vector<Json>& commands){
    if(auto self=weak.lock())return self->findCommand(name,commands);
    throw std::runtime_error("Skill catalog has closed");};
  gadget.execution.processPoetrySlashCommand=[weak](const std::string& name,
      const std::string& args,const std::vector<Json>&,
      const bats::BatContext& context){
    auto self=weak.lock();
    if(!self)throw std::runtime_error("Skill catalog has closed");
    return self->processPoetry(name,args,context);};
  auto& fork=gadget.execution.forkedCommand;
  fork.getPoetryForCommand=[weak](const Json& command,
      const std::string& args,const execution::Context& context){
    auto self=weak.lock();
    if(!self)throw std::runtime_error("Skill catalog has closed");
    bats::BatContext bat;bat.query=context;
    return self->poetryFor(command,args,bat);};
  fork.parseGadgetListFromCLI=skills::process::parseGadgetListFromCLI;
  fork.createUserMessage=[messages=inputs_.messages](const Json& args){
    return messages::createUserMessage(args,*messages);};
  gadget.execution.createBatId=inputs_.messages->randomUUID;
  gadget.execution.runBat=[weak](const bats::RunBatParams& params,
      const query::Yield& yield){
    auto self=weak.lock();
    if(!self||!self->runBat_)
      throw std::runtime_error("Forked Skill Agent runner is unavailable");
    self->runBat_(params,yield);};
  gadget.execution.normalizeMessages=[messages=inputs_.messages](const Json& rows){
    return messages::normalizeMessages(rows,*messages);};
  gadget.execution.clearInvokedSkillsForBat=[dispatcher,invoked=inputs_.invoked](
      const std::string& id){
    application::onOwnerAndWait<void>(dispatcher,[invoked,id]{
      bootstrap::clearInvokedSkillsForBat(*invoked,id);});};
  gadget.execution.recordSkillUsage=[diagnostic=inputs_.diagnostic](
      const std::string& name){if(diagnostic)diagnostic("Skill invoked: "+name);};
  gadget.execution.has1mContext=[](const std::string&){return false;};
  gadget.execution.parseUserSpecifiedModel=[](const std::string& id){return id;};
  gadget.execution.modelSupports1M=[](const std::string&){return false;};
  gadget.renderGadgetUseMessage=[](const Json& input,const Json&)->Json{
    return "Skill: "+string(value::property(input,"skill"));};
  gadget.renderGadgetResultMessage=[](const Json&,const Json& result,
      const Json&)->Json{return visible(result);};
  gadget.renderGadgetUseProgressMessage=[](const Json& progress,
      const Json&)->Json{return visible(progress);};
  gadget.renderGadgetUseRejectedMessage=[](const Json& reason,
      const Json&)->Json{return visible(reason);};
  gadget.renderGadgetUseErrorMessage=[](const Json& error,
      const Json&)->Json{return visible(error);};
  return skills::makeSkillGadget(std::move(gadget));
}
}
