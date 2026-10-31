#include "skills/SkillGadgetFactory.hpp"
#include "skills/usage/SkillUsage.hpp"
#include "connectors/Abort.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message){
  if(!value)throw std::runtime_error(message);
}
}
int main(){
  using batbox::Json;
  batbox::skills::SkillGadgetFactoryServices services;
  services.getProjectRoot=[]{return std::string("/project");};
  int rosterReads=0,discoveryReads=0,loads=0;
  services.getAllCommandsForValidation=[&](const Json&){
    ++rosterReads;return std::vector<Json>{};
  };
  int allowRuleReads=0;
  services.getRules=[&](const Json& context,const std::string& behavior){
    batbox::skills::RuleEntries rules;
    if(behavior=="allow")++allowRuleReads;
    if(behavior=="deny"&&batbox::value::property(context,"denyRemote")==true)
      rules.emplace_back("_canonical_:*",Json{{"source","policy"}});
    return rules;
  };
  services.execution.getAllCommands=[](const batbox::bats::BatContext&)
    ->std::vector<Json>{throw std::runtime_error("remote called local registry");};
  services.execution.lookup.findCommand=[](const std::string&,
      const std::vector<Json>&){return Json(nullptr);};
  batbox::skills::usage::State usageState;
  batbox::skills::usage::Services usageServices;
  Json config=Json::object();
  usageServices.nowMillis=[]{return std::int64_t{100000};};
  usageServices.getGlobalConfig=[&]{return config;};
  usageServices.saveGlobalConfig=[&](const std::function<Json(const Json&)>& update){
    config=update(config);
  };
  services.execution.recordSkillUsage=[&](const std::string& name){
    batbox::skills::usage::recordSkillUsage(name,usageState,usageServices);
  };
  auto& remote=services.execution.remote;
  remote.enabled=[]{return true;};
  remote.stripCanonicalPrefix=[](const std::string& name)->std::optional<std::string>{
    const std::string prefix="_canonical_";
    if(name.starts_with(prefix))return name.substr(prefix.size());
    return std::nullopt;
  };
  remote.getDiscoveredRemoteSkill=[&](const std::string& slug)
      ->std::optional<batbox::skills::remote::DiscoveredSkill>{
    ++discoveryReads;
    if(slug=="demo")return batbox::skills::remote::DiscoveredSkill{
      "https://example.invalid/demo.md"};
    return std::nullopt;
  };
  bool sawStop=false;
  remote.loadRemoteSkill=[&](const std::string& slug,
      const std::string& url,std::stop_token stop){
    ++loads;sawStop=stop.stop_requested();
    check(slug=="demo"&&url=="https://example.invalid/demo.md",
      "configured loader arguments");
    if(stop.stop_requested())throw batbox::ConnectorAbortError();
    return batbox::skills::remote::LoadedSkill{
      "/cache/demo/SKILL.md",
      "---\nname: demo\n---\nSee ${BATBOX_SKILL_DIR}/README and ${BATBOX_SESSION_ID}.",
      true,3};
  };
  remote.getFlightId=[]{return std::string("session-1");};
  remote.currentBatId=[]{return std::optional<std::string>("worker-1");};
  Json invoked=nullptr;
  remote.addInvokedSkill=[&](const std::string& name,const std::string& path,
      const std::string& content,const std::optional<std::string>& batId){
    invoked={{"name",name},{"path",path},{"content",content},
      {"agentId",batId?Json(*batId):Json(nullptr)}};
  };
  remote.messageServices.randomUUID=[]{return std::string("message-1");};
  remote.messageServices.nowISO=[]{return std::string("2026-01-01T00:00:00.000Z");};
  services.renderGadgetUseMessage=[](const Json&,const Json&){return Json(nullptr);};
  services.renderGadgetResultMessage=[](const Json&,const Json&,const Json&){
    return Json(nullptr);
  };
  services.renderGadgetUseProgressMessage=[](const Json&,const Json&){return Json(nullptr);};
  services.renderGadgetUseRejectedMessage=services.renderGadgetUseProgressMessage;
  services.renderGadgetUseErrorMessage=services.renderGadgetUseProgressMessage;
  const auto gadget=batbox::skills::makeSkillGadget(std::move(services));
  const Json input={{"skill","/_canonical_demo"}};
  check(gadget.validateInput(input,{}).get().result,"discovered remote validates");
  check(rosterReads==0,"remote validation bypasses local registry");
  const auto denied=gadget.checkPermissions(input,Json{{"denyRemote",true}}).get();
  check(denied.at("behavior")=="deny","deny rules precede remote allow");
  const auto allowed=gadget.checkPermissions(input,{}).get();
  check(allowed.at("behavior")=="allow","remote permission allowed");
  check(allowRuleReads==0,"remote branch skips allow-rule lookup");
  std::stop_source stop;stop.request_stop();
  batbox::gadgets::GadgetCallContext callContext;
  callContext.stopToken=stop.get_token();
  callContext.batContext=std::make_shared<batbox::bats::BatContext>();
  const Json parent={{"message",{{"content",Json::array({
    Json{{"type","tool_use"},{"id","tool-1"},{"name","Skill"}}})}}}};
  bool aborted=false;
  try{(void)gadget.call(input,callContext,{},parent,{}).get();}
  catch(const batbox::ConnectorAbortError&){aborted=true;}
  check(aborted&&loads==1&&sawStop,"remote fetch cancellation remains typed");
  callContext.stopToken={};
  const auto result=gadget.call(input,callContext,{},parent,{}).get();
  check(loads==2,"remote loader retried after cancellation");
  check(discoveryReads>=2,"discovery rechecked at call");
  check(result.newMessages&&result.newMessages->size()==1,"remote message emitted");
  const auto& message=result.newMessages->at(0);
  check(message.at("sourceToolUseID")=="tool-1","remote message tagged");
  const auto content=message.at("message").at("content").get<std::string>();
  check(content=="Base directory for this skill: /cache/demo\n\n"
    "See /cache/demo/README and session-1.","frontmatter and placeholders");
  check(invoked.at("content")==content&&invoked.at("agentId")=="worker-1",
    "compaction invoked-skill state");
  check(config.at("skillUsage").at("_canonical_demo").at("usageCount")==1,
    "native local usage persisted");
  std::cout<<"SkillTool remote factory native dispatch passed\n";
}
