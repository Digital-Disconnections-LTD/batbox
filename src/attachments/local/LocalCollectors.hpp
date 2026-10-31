#pragma once
#include "attachments/Aggregate.hpp"
#include "attachments/files/FileAttachments.hpp"
#include "bootstrap/FlightTransitions.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::attachments::local {
struct MissionBatch {
  Json attachments=Json::array(),updatedOffsets=Json::object(),evictedIds=Json::array();
};
struct Services {
  event_loop::Loop& loop;
  explicit Services(event_loop::Loop& owner):loop(owner){}
  std::shared_ptr<files::Services> files;
  std::function<app_state::AppStateRef()> getAppState;
  std::function<std::string(const std::optional<std::string>&)> planFilePath;
  std::function<std::optional<Json>(const std::optional<std::string>&)> getPlan;
  bootstrap::FlightTransitions* transitions=nullptr;
  std::function<std::string()> flightId,missionListId;
  std::function<event_loop::Deferred<Json>(const std::string&)> listMissions;
  std::function<bool()> todoV2Enabled,internalUser,verifyPlanEnabled;
  std::function<event_loop::Deferred<Json>()> getNewDiagnostics;
  std::function<Json()> checkLspDiagnostics;
  std::function<void()> clearLspDiagnostics;
  std::function<void(std::exception_ptr)> logError;
  std::function<void(const std::string&)> logDebug;
  std::function<event_loop::Deferred<MissionBatch>(const app_state::AppStateRef&)>
    generateMissionAttachments;
  std::function<void(const Json&,const Json&)> applyMissionOffsetsAndEvictions;
  std::function<std::string(const std::string&)> missionOutputPath;
  std::function<Json(const std::string&,const std::string&,
    const std::string&,int)> structuredPatch;
  std::function<bool()> compactLinePrefixEnabled;
  std::function<event_loop::Deferred<Json>(const std::string&)>
    readImageWithTokenBudget;
  std::function<bool(std::exception_ptr)> isENOENT;
  std::string todoWriteGadgetName="TodoWrite",briefGadgetName="Brief",
    missionCreateGadgetName="TaskCreate",missionUpdateGadgetName="TaskUpdate",
    bashGadgetName="Bash";
};

std::string getSnippetForTwoFileDiff(const std::string& before,
  const std::string& after,const Services& services);
event_loop::Deferred<Json> getChangedFiles(execution::Context& context,
  const Services& services);
Json getPlanModeAttachments(const Json& messages,
  const execution::Context& context,const Services& services);
Json getPlanModeExitAttachment(const execution::Context& context,
  const Services& services);
Json getTodoReminderAttachments(const Json& messages,
  const execution::Context& context,const Services& services);
event_loop::Deferred<Json> getMissionReminderAttachments(const Json& messages,
  const execution::Context& context,const Services& services);
event_loop::Deferred<Json> getDiagnosticAttachments(
  const execution::Context& context,const Services& services);
Json getLspDiagnosticAttachments(const execution::Context& context,
  const Services& services);
event_loop::Deferred<Json> getUnifiedMissionAttachments(const Services& services);
Json getVerifyPlanReminderAttachment(const Json& messages,
  const Services& services);
void bindLocalCollectors(attachments::Services& aggregation,Services services);
Services makeIndigenousLocalServices(event_loop::Loop& loop,
  std::shared_ptr<files::Services> fileServices,
  const gadgets::files::FileReadServices& readServices);
} // namespace batbox::attachments::local
