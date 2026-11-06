#pragma once
#include "attachment_triggers/Attachments.hpp"
#include "interactive/background/NotificationAttachments.hpp"
#include "interactive/submit/CommandQueue.hpp"
#include "model/ModelWrappers.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::attachments {
struct Input {
  std::optional<std::string> text;
  Json ideSelection=Json();
  interactive::submit::CommandList queuedCommands;
  Json messages=Json();
  std::optional<std::string> querySource;
};
using Collector=std::function<event_loop::Deferred<Json>(
  const Input&,execution::Context&)>;

struct Collectors {
  Collector atMentionedFiles,mcpResources,batMentions;
  Collector dateChange,deferredGadgetsDelta,batListingDelta,
    mcpInstructionsDelta,changedFiles,skillListing,planMode,planModeExit,
    todoReminders,missionReminders,batPendingMessages,criticalSystemReminder;
  Collector selectedLinesIde,openedFileIde,outputStyle,diagnostics,
    lspDiagnostics,unifiedMissions,asyncHookResponses,tokenUsage;
  // Optional: main-thread changes to the files behind a stable head message.
  Collector headFileChanges;
};

struct Services {
  event_loop::Loop& loop;
  Collectors collectors;
  std::shared_ptr<attachment_triggers::Services> triggers;
  interactive::background::AttachmentServices notifications;
  std::function<bool()> isTodoV2Enabled,disableAttachments,simpleMode;
  std::function<void(std::exception_ptr)> logError;
  std::function<void(const std::string&,std::exception_ptr)> logDiagnostic;
  std::function<std::string()> randomUuid,nowIso;
};

event_loop::Deferred<Json> getAttachments(Input input,
  execution::Context& gadgetUseContext,const Services& services);

struct MessageCursor {
  std::function<event_loop::Deferred<std::optional<model::MessageRef>>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
MessageCursor getAttachmentMessages(Input input,
  execution::Context& gadgetUseContext,const Services& services);
} // namespace batbox::attachments
