#pragma once
#include "bats/RunBat.hpp"
#include "event_loop/Deferred.hpp"
#include "messages/Messages.hpp"
#include "user_input/SlashDispatch.hpp"
#include <chrono>

namespace batbox::commands::fork {
using BatMessage=std::function<void(const query::MessageRef&)>;
using BatRunner=std::function<event_loop::Deferred<event_loop::Unit>(
  bats::RunBatParams,BatMessage)>;

// The launcher chooses the application's bat executor. Its callbacks and
// bat services must be safe on that executor; yielded messages and settlement
// are delivered to the owner loop by bindIndigenousRunner.
BatRunner bindIndigenousRunner(event_loop::Loop& loop,
  std::shared_ptr<bats::RunBatPreflightServices> preflight,
  std::shared_ptr<bats::RunBatSetupServices> setup,
  std::shared_ptr<bats::RunBatLoopServices> batLoop,
  std::function<void(std::function<void()>)> launch);

struct Services {
  event_loop::Loop& loop;
  messages::Services& messages;
  Services(event_loop::Loop& loop,messages::Services& messages):
    loop(loop),messages(messages){}
  bats::BatContext context;
  bats::CommandServices prepare;
  std::function<std::future<Json>(const Json&)> canUseGadget;
  BatRunner runBat;
  std::function<std::string()> createBatId,randomUUID,nowISO;
  std::function<std::int64_t()> nowMs;
  std::function<bool(const std::string&)> feature;
  std::function<gadgets::Gadgets()> refreshGadgets;
  std::function<std::optional<Json>()> getWorkload;
  std::function<void(const Json&)> enqueuePendingNotification;
  // Indigenous progress projection, rendered by the app's FTXUI view.
  std::function<void(const Json&)> showProgress;
  std::function<void()> clearProgress;
  std::function<void(const std::string&)> logDebug,logError;
};

event_loop::Deferred<user_input::InputResult> executeForkedSlashCommand(
  const Json& command,const std::string& args,const Json& preceding,
  Services& services);

user_input::CommandEntry bindForkedCommand(Json descriptor,
  std::shared_ptr<Services> services);
} // namespace batbox::commands::fork
