#include "skills/SkillGadgetFactory.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
}
int main(){
  using batbox::Json;
  const Json command={{"type","prompt"},{"name","review"},
    {"description","Review this work"}};
  batbox::skills::SkillGadgetFactoryServices services;
  services.getProjectRoot=[](){return std::string("/project");};
  services.getAllCommandsForValidation=[command](const Json&){
    return std::vector<Json>{command};};
  services.getRules=[](const Json&,const std::string&){
    return batbox::skills::RuleEntries{};};
  services.execution.getAllCommands=[command](const batbox::bats::BatContext&){
    return std::vector<Json>{command};};
  services.execution.lookup.findCommand=[](const std::string& name,
    const std::vector<Json>& commands){
    for(const auto& item:commands)if(item.at("name")==name)return item;
    return Json(nullptr);
  };
  int usageCalls=0;
  services.execution.recordSkillUsage=[&](const std::string& name){
    check(name=="review","usage name");++usageCalls;
  };
  services.execution.processPoetrySlashCommand=[](const std::string& name,
    const std::string& args,const std::vector<Json>&,
    const batbox::bats::BatContext&){
    check(name=="review"&&args=="42","processPromptSlashCommand arguments");
    return Json{{"shouldQuery",true},{"allowedTools",Json::array({"Read"})},
      {"model","standard"},{"messages",Json::array({{{"type","user"},
        {"message",{{"content","Review instructions"}}}}})}};
  };
  services.execution.has1mContext=[](const std::string& model){
    return model.ends_with("[1m]");};
  services.execution.parseUserSpecifiedModel=[](const std::string& model){return model;};
  services.execution.modelSupports1M=[](const std::string&){return true;};
  services.renderGadgetUseMessage=[](const Json&,const Json&){return Json(nullptr);};
  services.renderGadgetResultMessage=[](const Json&,const Json&,const Json&){
    return Json(nullptr);};
  services.renderGadgetUseProgressMessage=[](const Json&,const Json&){return Json(nullptr);};
  services.renderGadgetUseRejectedMessage=services.renderGadgetUseProgressMessage;
  services.renderGadgetUseErrorMessage=services.renderGadgetUseProgressMessage;
  const auto gadget=batbox::skills::makeSkillGadget(std::move(services));
  const Json input={{"skill","/review"},{"args","42"}};
  check(gadget.name=="Skill","factory name");
  check(gadget.parseInput(Json{{"skill","review"},{"extra",5}})==
    Json{{"skill","review"}},"Zod input stripping");
  check(gadget.validateInput(input,{}).get().result,"validation");
  check(gadget.checkPermissions(input,{}).get().at("behavior")=="allow","permission");
  batbox::gadgets::GadgetCallContext callContext;
  callContext.batContext=std::make_shared<batbox::bats::BatContext>();
  callContext.batContext->query.data={{"options",{{"mainLoopModel","premium[1m]"}}}};
  callContext.batContext->getAppState=[](){return Json{
    {"toolPermissionContext",{{"alwaysAllowRules",{{"command",Json::array({"Bash"})}}}}}};};
  const Json parent={{"message",{{"id","parent-1"},{"content",Json::array({
    {{"type","tool_use"},{"name","Skill"},{"id","tool-1"}}})}}}};
  const auto output=gadget.call(input,callContext,{},parent,{}).get();
  check(usageCalls==1,"local skill usage recorded once");
  check(output.data.at("commandName")=="review","call output");
  check(output.newMessages&&output.newMessages->size()==1&&
    output.newMessages->at(0).at("sourceToolUseID")=="tool-1","message tagging");
  check(output.appStateModifier&&output.contextModifier,"typed modifiers");
  const auto changedState=output.appStateModifier(callContext.batContext->getAppState());
  check(changedState["toolPermissionContext"]["alwaysAllowRules"]["command"]==
    Json::array({"Bash","Read"}),"live allowed tools");
  const auto changedOptions=output.contextModifier(callContext.batContext->query.data);
  check(changedOptions["options"]["mainLoopModel"]=="standard[1m]",
    "model window carryover");
  check(gadget.mapGadgetResultToGadgetResultBlockParam(output.data,"tool-1")["content"]==
    "Launching skill: review","tool result mapping");
  batbox::gadgets::GadgetCallContext missing;
  bool rejected=false;
  try{(void)gadget.call(input,missing,{},parent,{}).get();}
  catch(const std::invalid_argument&){rejected=true;}
  check(rejected,"missing native context must fail");
  std::cout<<"SkillTool factory native dispatch passed\n";
}
