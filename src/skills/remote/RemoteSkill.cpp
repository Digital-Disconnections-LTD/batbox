#include "skills/remote/RemoteSkill.hpp"
#include "instructions/Frontmatter.hpp"
#include "connectors/Abort.hpp"
#include "skills/SkillGadgetExecution.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::skills::remote {
std::string remoteSkillDirectory(const std::string& path,bool isWindows);
std::string replaceLiteralWithJsSubstitution(const std::string& input,
  const std::string& marker,const std::string& replacement);
namespace {
template<class Fn>void need(const Fn& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Missing remote skill binding: ")+name);
}
}
std::optional<std::string> remoteSlugForCommand(const std::string& name,
  Services& services){
  need(services.enabled,"enabled");
  if(!services.enabled())return std::nullopt;
  need(services.stripCanonicalPrefix,"stripCanonicalPrefix");
  return services.stripCanonicalPrefix(name);
}
std::optional<gadgets::ValidationResult> validateRemoteSkill(
  const std::string& commandName,Services& services){
  const auto slug=remoteSlugForCommand(commandName,services);
  if(!slug)return std::nullopt;
  need(services.getDiscoveredRemoteSkill,"getDiscoveredRemoteSkill");
  if(!services.getDiscoveredRemoteSkill(*slug)){
    gadgets::ValidationResult invalid;
    invalid.result=false;
    invalid.message="Remote skill "+*slug+
      " was not discovered in this session. Use DiscoverSkills to find remote skills first.";
    invalid.errorCode=6;
    return invalid;
  }
  return gadgets::ValidationResult{};
}
gadgets::GadgetResult executeRemoteSkill(const std::string& slug,
  const std::string& commandName,const Json& parentMessage,
  Services& services,std::stop_token stopToken){
  need(services.getDiscoveredRemoteSkill,"getDiscoveredRemoteSkill");
  const auto meta=services.getDiscoveredRemoteSkill(slug);
  if(!meta)throw std::runtime_error("Remote skill "+slug+
    " was not discovered in this session. Use DiscoverSkills to find remote skills first.");
  need(services.loadRemoteSkill,"loadRemoteSkill");
  LoadedSkill loaded;
  try{loaded=services.loadRemoteSkill(slug,meta->url,stopToken);}
  catch(const ConnectorAbortError&){throw;}
  catch(const std::exception& error){
    if(stopToken.stop_requested())throw ConnectorAbortError();
    throw std::runtime_error("Failed to load remote skill "+slug+": "+error.what());
  }
  const auto& skillPath=loaded.skillPath;
  const auto& content=loaded.content;
  need(services.recordSkillUsage,"recordSkillUsage");
  services.recordSkillUsage(commandName);
  if(services.logDebug){
    const auto stash=loaded.stashHit?"true":"false";
    services.logDebug("SkillTool loaded remote skill "+slug+
      " (cacheHit="+stash+", "+value::numberToString(loaded.latencyMs)+"ms, "+
      std::to_string(value::stringLength(content))+" chars)");
  }
  const auto body=instructions::parseFrontmatter(content,skillPath).content;
  const auto directory=remoteSkillDirectory(skillPath,services.isWindows);
  std::string finalContent="Base directory for this skill: "+directory+"\n\n"+body;
  finalContent=replaceLiteralWithJsSubstitution(finalContent,
    services.skillDirPlaceholder,directory);
  need(services.getFlightId,"getSessionId");
  finalContent=replaceLiteralWithJsSubstitution(finalContent,
    services.flightIdPlaceholder,services.getFlightId());
  need(services.addInvokedSkill,"addInvokedSkill");
  need(services.currentBatId,"currentAgentId");
  services.addInvokedSkill(commandName,skillPath,finalContent,
    services.currentBatId());
  const auto id=skills::getSkillGadgetUseIDFromParentMessage(parentMessage,"Skill");
  const auto message=messages::createUserMessage(
    Json{{"content",finalContent},{"isMeta",true}},services.messageServices);
  gadgets::GadgetResult result;
  result.data={{"success",true},{"commandName",commandName},{"status","inline"}};
  result.newMessages=skills::tagSkillMessagesWithGadgetUseID({message},id);
  return result;
}
}
