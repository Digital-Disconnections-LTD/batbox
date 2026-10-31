#pragma once
#include "user_input/TextPoetry.hpp"
#include "event_loop/Deferred.hpp"

namespace batbox::user_input {
struct BashInputServices {
  event_loop::Loop& loop;
  messages::Services& messages;
  std::function<bool()> isPowerShellGadgetEnabled;
  std::function<std::string()> resolveDefaultShell;
  std::function<void(const std::string&,const std::optional<Json>&)> showProgress;
  std::function<void()> clearProgress;
  std::function<event_loop::Deferred<Json>(bool usePowerShell,const Json& input,
    const std::function<void(const Json&)>& onProgress)> callShell;
  std::function<event_loop::Deferred<Json>(bool usePowerShell,const Json& data,
    const std::string& id)> processGadgetResultBlock;
};
event_loop::Deferred<InputResult> processBashCommand(const std::string& input,
  const Json& preceding,const Json& attachments,BashInputServices& services);
} // namespace batbox::user_input
