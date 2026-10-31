#include "flights/MetadataOperations.hpp"
namespace batbox::flights {
void MetadataOperations::saveCustomTitle(const std::string& flight,const std::string& title,const std::optional<std::string>& path) {
  services.append(path ? *path : services.paths->getTranscriptPathForFlight(flight),Json{{"type","custom-title"},{"customTitle",title},{"sessionId",flight}});
  if(flight==services.getFlightId())services.project->currentFlightTitle=title;
}
void MetadataOperations::saveAiGeneratedTitle(const std::string& flight,const std::string& title) {
  services.append(services.paths->getTranscriptPathForFlight(flight),Json{{"type","ai-title"},{"aiTitle",title},{"sessionId",flight}});
}
void MetadataOperations::saveMissionSummary(const std::string& flight,const std::string& summary) {
  services.append(services.paths->getTranscriptPathForFlight(flight),Json{{"type","task-summary"},{"summary",summary},{"sessionId",flight},{"timestamp",services.nowIso()}});
}
void MetadataOperations::saveTag(const std::string& flight,const std::string& tag,const std::optional<std::string>& path) {
  services.append(path ? *path : services.paths->getTranscriptPathForFlight(flight),Json{{"type","tag"},{"tag",tag},{"sessionId",flight}});
  if(flight==services.getFlightId())services.project->currentFlightTag=tag;
}
void MetadataOperations::linkFlightToPR(const std::string& flight,const Json& number,const std::string& url,
  const std::string& repository,const std::optional<std::string>& path) {
  services.append(path ? *path : services.paths->getTranscriptPathForFlight(flight),Json{{"type","pr-link"},{"sessionId",flight},{"prNumber",number},{"prUrl",url},{"prRepository",repository},{"timestamp",services.nowIso()}});
  if(flight==services.getFlightId()) {
    services.project->currentFlightPrNumber=number;services.project->currentFlightPrUrl=url;services.project->currentFlightPrRepository=repository;
  }
}
void MetadataOperations::saveBatName(const std::string& flight,const std::string& name,const std::optional<std::string>& path) {
  services.append(path ? *path : services.paths->getTranscriptPathForFlight(flight),Json{{"type","agent-name"},{"agentName",name},{"sessionId",flight}});
  if(flight==services.getFlightId()){services.project->currentFlightBatName=name;services.updateFlightName(name);}
}
void MetadataOperations::saveBatColor(const std::string& flight,const std::string& color,const std::optional<std::string>& path) {
  services.append(path ? *path : services.paths->getTranscriptPathForFlight(flight),Json{{"type","agent-color"},{"agentColor",color},{"sessionId",flight}});
  if(flight==services.getFlightId())services.project->currentFlightBatColor=color;
}
}
