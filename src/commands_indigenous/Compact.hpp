#pragma once
#include "event_loop/Deferred.hpp"
#include "user_input/SlashDispatch.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::commands_indigenous {
struct CompactServices {
  event_loop::Loop& loop;
  explicit CompactServices(event_loop::Loop& owner):loop(owner){}
  std::function<Json()> getMessages;
  std::function<Json(const Json&)> afterCompactBoundary;
  std::function<bool()> aborted,reactiveOnly;
  std::function<bool(const std::string&)> feature;
  std::function<event_loop::Deferred<std::optional<Json>>(
    const Json&,const std::optional<std::string>&)> tryFlightMemoryCompaction;
  std::function<event_loop::Deferred<Json>(const Json&)> microcompactMessages;
  std::function<event_loop::Deferred<Json>(const Json&)> stashSharingParams;
  std::function<event_loop::Deferred<Json>(const Json&,const Json&,
    const std::string&)> compactConversation;
  std::function<event_loop::Deferred<Json>(const std::string&)> preCompactHooks;
  std::function<event_loop::Deferred<Json>(const Json&,const Json&,
    const std::string&)> reactiveCompact;
  std::function<void(const Json&)> onCompactProgress;
  std::function<void(const std::optional<std::string>&)> setSdkStatus;
  std::function<void(const std::string&)> setStreamMode;
  std::function<void()> resetResponseLength;
  std::function<void()> clearUserContextStash,runPostCompactCleanup,
    markPostCompaction,suppressCompactWarning,clearLastSummarizedMessageId;
  std::function<void(const std::string&,const std::optional<std::string>&)>
    notifyCompaction;
  std::function<void(std::exception_ptr)> logError;
  std::function<std::optional<std::string>()> batId;
  std::function<std::string()> querySource,getUpgradeTip,getExpandShortcut;
  std::function<bool()> verbose;
  std::function<std::string(const std::string&)> dim;
  std::string notEnoughMessage="Not enough messages to compact.";
  std::string incompleteMessage="Conversation too long. Press esc twice to go up a few messages and try again.";
};
event_loop::Deferred<Json> runCompactCommand(const std::string& args,
  CompactServices services);
user_input::CommandEntry makeCompactCommand(
  std::shared_ptr<CompactServices> services);
} // namespace batbox::commands_indigenous
