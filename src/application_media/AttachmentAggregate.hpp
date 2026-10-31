#pragma once
#include "application_media/Attachments.hpp"
#include "application_media/Bindings.hpp"
#include "attachments/Aggregate.hpp"
#include "attachments/local/LocalCollectors.hpp"
#include "bootstrap/Costs.hpp"
#include "bootstrap/PoetryState.hpp"
#include "bootstrap/FlightTransitions.hpp"
#include "query/AsyncGadgetTurn.hpp"
#include <future>
#include <set>

namespace batbox::application_media {
struct AggregateInputs {
  AttachmentInputs files;
  std::shared_ptr<Bundle> media;
  std::function<bool(const std::string&)> feature;
  std::function<void(const std::string&)> log;
  std::function<std::string()> cwd,flightId,localIsoDate;
  std::function<Json()> settings,skillCommands,mcpClients,mcpResources;
  std::function<bool(const std::string&)> modelSupportsGadgetReference;
  std::function<std::future<Json>(const std::string& server,
    const std::string& uri,std::stop_token)> readMcpResource;
  std::function<std::optional<std::string>(const Json&)> connectedIdeName;
  std::function<event_loop::Deferred<Json>()> getNewDiagnostics,
    getAsyncHookResponses;
  std::function<Json()> checkLspDiagnostics;
  std::function<void()> clearLspDiagnostics;
  std::function<event_loop::Deferred<attachments::local::MissionBatch>(
    const app_state::AppStateRef&)> generateMissionAttachments;
  std::function<void(const Json&,const Json&)> applyMissionOffsetsAndEvictions;
  std::function<std::string(const std::string&)> missionOutputPath;
  std::function<std::string(const std::optional<std::string>&)> planFilePath;
  std::function<std::optional<Json>(const std::optional<std::string>&)> getPlan;
  std::function<Json(const std::string&,const std::string&,
    const std::string&,int)> structuredPatch;
  std::function<event_loop::Deferred<Json>(const std::string&)> listMissions;
  std::function<std::string()> missionListId;
  std::function<bool()> isTodoV2Enabled,isBatSwarmsEnabled,
    ideAvailable,lspAvailable,internalUser,batListInMessages,
    deferredGadgetsEnabled,mcpInstructionsDeltaEnabled,
    tokenUsageAttachmentEnabled,ultrathinkEnabled;
  std::function<double(const std::string&)> effectiveContextWindow;
  std::function<Json(const Json&,const Json&,const std::string&)>
    filterDeniedBats;
  std::function<std::string()> subscriptionType;
  std::function<Json(const std::string&)> drainBatPendingMessages;
  std::shared_ptr<attachment_triggers::Services> triggers;
  std::shared_ptr<bootstrap::PoetryState> poetryState;
  std::shared_ptr<bootstrap::CostState> costState;
  std::shared_ptr<bootstrap::FlightTransitions> transitions;
  explicit AggregateInputs(event_loop::Loop& owner):files(owner){}
};

// Binds attachment collection to the executable. Optional
// features retain explicit imports; an enabled missing collector is logged by
// the aggregate collector boundary and does not become a false result.
class AggregateProvider final:public std::enable_shared_from_this<AggregateProvider>{
public:
  static std::shared_ptr<AggregateProvider> create(AggregateInputs inputs);
  event_loop::Deferred<Json> collectPoetry(const std::string& text,
    const Json& ideSelection,const Json& images,const Json& messages,
    const std::optional<std::string>& querySource);
  query::AsyncMessageCursor turnCursor(const Json& queuedCommands,
    const execution::Context& context,const Json& messages,
    const std::string& querySource);
  void suppressNextSkillListing(){suppressNextSkillListing_=true;}
private:
  explicit AggregateProvider(AggregateInputs inputs):inputs_(std::move(inputs)){}
  attachments::Services services(
    const std::shared_ptr<gadgets::files::FileReadServices>& read);
  void bindRemaining(attachments::Services& services);
  void observeMode();
  AggregateInputs inputs_;
  std::map<std::string,std::set<std::string>> sentSkillNames_;
  bool suppressNextSkillListing_=false;
  std::optional<std::string> previousMode_;
};
} // namespace batbox::application_media
