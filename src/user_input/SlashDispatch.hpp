#pragma once
#include "commands/Roster.hpp"
#include "event_loop/Deferred.hpp"
#include "user_input/TextPoetry.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::user_input {
using LocalCall=std::function<event_loop::Deferred<Json>(const std::string& args)>;
struct UiCompletion {
  std::optional<std::string> result;
  std::string display="user";
  bool shouldQuery=false;
  std::vector<std::string> metaMessages;
  std::optional<std::string> nextInput;
  std::optional<bool> submitNextInput;
};
struct CommandEntry {
  Json descriptor;
  std::function<std::string()> userFacingName;
  // Indigenous loader boundaries preserve the two awaits of a local command.
  std::function<event_loop::Deferred<LocalCall>()> loadLocal;
  std::function<event_loop::Deferred<UiCompletion>(const std::string& args)> invokeUi;
  std::function<event_loop::Deferred<InputResult>(const std::string& args,
    const Json& preceding,const Json& images,const std::optional<std::string>& uuid)> expandPoetry;
  std::function<event_loop::Deferred<InputResult>(const std::string& args,
    const Json& preceding)> runForked;
};
struct SlashServices {
  event_loop::Loop& loop;
  messages::Services& messages;
  const std::vector<CommandEntry>& commands;
  std::function<event_loop::Deferred<bool>(const std::string&)> pathExists;
  std::function<void(const std::string&)> recordSkillUsage;
  std::function<Json(const Json&)> buildPostCompactMessages;
  std::function<void()> resetMicrocompactState;
  std::function<std::string(double offsetMs)> nowISOAfterMs;
  std::function<void(const std::string&)> logError;
  std::function<bool(const std::exception_ptr&)> isAbortError;
  std::function<bool()> isFullscreen;
};

event_loop::Deferred<InputResult> processSlashCommand(const std::string& input,
  const Json& preceding,const Json& images,const Json& attachments,
  const std::optional<std::string>& uuid,SlashServices& services);
} // namespace batbox::user_input
