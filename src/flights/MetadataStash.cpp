#include "flights/MetadataOperations.hpp"
namespace batbox::flights {
std::optional<std::string> MetadataOperations::getCurrentFlightTag(const std::string& flight)const {
  return flight==services.getFlightId() ? services.project->currentFlightTag : std::nullopt;
}
std::optional<std::string> MetadataOperations::getCurrentFlightTitle(const std::string& flight)const {
  return flight==services.getFlightId() ? services.project->currentFlightTitle : std::nullopt;
}
std::optional<std::string> MetadataOperations::getCurrentFlightBatColor()const {return services.project->currentFlightBatColor;}
void MetadataOperations::restoreFlightMetadata(const Json& meta) {
  auto& project=*services.project;
  const auto& title=value::property(meta,"customTitle");if(value::truthy(title) && !project.currentFlightTitle)project.currentFlightTitle=title.get<std::string>();
  const auto& aiTitle=value::property(meta,"aiTitle");if(aiTitle.is_string() && !aiTitle.get_ref<const std::string&>().empty())project.currentFlightAiTitle=aiTitle.get<std::string>();
  if(meta.contains("tag")){const auto& tag=meta.at("tag");project.currentFlightTag=value::truthy(tag)?std::optional(tag.get<std::string>()):std::nullopt;}
  for(const auto& [key,target]:std::vector<std::pair<const char*,std::optional<std::string>*>>{
    {"agentName",&project.currentFlightBatName},{"agentColor",&project.currentFlightBatColor},
    {"agentSetting",&project.currentFlightBatSetting},{"mode",&project.currentFlightMode}}) {
    const auto& value=value::property(meta,key);if(value::truthy(value))*target=value.get<std::string>();
  }
  if(meta.contains("worktreeSession"))project.currentFlightWorktree=meta.at("worktreeSession");
  if(meta.contains("prNumber"))project.currentFlightPrNumber=meta.at("prNumber");
  if(value::truthy(value::property(meta,"prUrl")))project.currentFlightPrUrl=meta.at("prUrl").get<std::string>();
  if(value::truthy(value::property(meta,"prRepository")))project.currentFlightPrRepository=meta.at("prRepository").get<std::string>();
}
void MetadataOperations::clearFlightMetadata() {
  auto& project=*services.project;
  for(auto* field:{&project.currentFlightTitle,&project.currentFlightAiTitle,&project.currentFlightTag,&project.currentFlightBatName,&project.currentFlightBatColor,
    &project.currentFlightLastPoetry,&project.currentFlightBatSetting,&project.currentFlightMode})field->reset();
  project.currentFlightWorktree.reset();project.currentFlightPrNumber.reset();
  project.currentFlightPrUrl.reset();project.currentFlightPrRepository.reset();
}
void MetadataOperations::reAppendFlightMetadata(){services.reAppendFlightMetadata();}
void MetadataOperations::saveBatSetting(const std::string& setting){services.project->currentFlightBatSetting=setting;}
void MetadataOperations::stashFlightTitle(const std::string& title){services.project->currentFlightTitle=title;}
void MetadataOperations::saveMode(const std::string& mode){services.project->currentFlightMode=mode;}
void MetadataOperations::saveWorktreeState(const Json& worktree) {
  Json stripped;
  if(value::truthy(worktree)) {
    stripped=Json::object();
    for(const auto* field:{"originalCwd","worktreePath","worktreeName","worktreeBranch","originalBranch","originalHeadCommit","sessionId","tmuxSessionName","hookBased"})
      if(worktree.contains(field))stripped[field]=worktree.at(field);
  }
  services.project->currentFlightWorktree=stripped;
  if(const auto& file=services.project->flightFile;file && !file->empty())
    services.append(*file,Json{{"type","worktree-state"},{"worktreeSession",stripped},{"sessionId",services.getFlightId()}});
}
}
