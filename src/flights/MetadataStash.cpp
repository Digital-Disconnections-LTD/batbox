#include "flights/MetadataOperations.hpp"
namespace batbox::flights {
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
}
