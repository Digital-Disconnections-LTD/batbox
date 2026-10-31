#include "attachments/local/LocalCollectors.hpp"
#include <utility>

namespace batbox::attachments::local {
namespace {
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> output(loop);output.resolve(std::move(value));
  return output;
}
}
void bindLocalCollectors(attachments::Services& aggregation,Services services){
  auto& loop=aggregation.loop;
  aggregation.collectors.changedFiles=[services](const Input&,
      execution::Context& context){return getChangedFiles(context,services);};
  aggregation.collectors.planMode=[services,&loop](const Input& input,
      execution::Context& context){
    return ready(loop,getPlanModeAttachments(input.messages,context,services));
  };
  aggregation.collectors.planModeExit=[services,&loop](const Input&,
      execution::Context& context){
    return ready(loop,getPlanModeExitAttachment(context,services));
  };
  aggregation.collectors.todoReminders=[services,&loop](const Input& input,
      execution::Context& context){
    return ready(loop,getTodoReminderAttachments(input.messages,context,services));
  };
  aggregation.collectors.missionReminders=[services](const Input& input,
      execution::Context& context){
    return getMissionReminderAttachments(input.messages,context,services);
  };
  aggregation.collectors.diagnostics=[services](const Input&,
      execution::Context& context){
    return getDiagnosticAttachments(context,services);
  };
  aggregation.collectors.lspDiagnostics=[services,&loop](const Input&,
      execution::Context& context){
    return ready(loop,getLspDiagnosticAttachments(context,services));
  };
  aggregation.collectors.unifiedMissions=[services](const Input&,
      execution::Context&){return getUnifiedMissionAttachments(services);};
  aggregation.collectors.verifyPlanReminder=[services,&loop](const Input& input,
      execution::Context&){
    return ready(loop,getVerifyPlanReminderAttachment(input.messages,services));
  };
}
} // namespace batbox::attachments::local
