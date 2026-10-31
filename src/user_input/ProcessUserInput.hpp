#pragma once
#include "instructions/hooks/runtime/StopPoetryWrappers.hpp"
#include "user_input/SlashDispatch.hpp"
#include <stop_token>

namespace batbox::user_input {
struct UserInputRequest {
  Json input;
  std::stop_token abort;
  std::optional<std::string> preExpansionInput;
  std::string mode="prompt";
  Json pastedContents=Json();
  Json ideSelection=Json();
  Json messages=Json();
  std::optional<std::string> uuid,querySource;
  bool isAlreadyProcessing=false,skipSlashCommands=false,bridgeOrigin=false;
  bool isMeta=false,skipAttachments=false,isNonInteractiveFlight=false;
};
struct UserInputServices {
  event_loop::Loop& loop;
  messages::Services& messages;
  SlashServices& slash;
  std::function<Json()> getAppState;
  std::function<void(const std::string&)> setUserInputOnProcessing;
  std::function<void(const std::string&)> queryCheckpoint;
  std::function<event_loop::Deferred<Json>(const Json&)> resizeImageBlock;
  std::function<event_loop::Deferred<Json>(const Json&)> storeImages;
  std::function<std::optional<std::string>(const Json&,
    const std::optional<std::string>&)> createImageMetadataText;
  std::function<event_loop::Deferred<Json>(const std::string&,const Json&,
    const Json&,const Json&,const std::optional<std::string>&)> getAttachmentMessages;
  std::function<event_loop::Deferred<InputResult>(const std::string&,
    const Json&,const Json&)> processBashCommand;
  std::function<bool(const Json&)> isBridgeSafeCommand;
  std::function<bool(const std::string&)> feature;
  std::function<bool(const std::string&)> hasUltraplanKeyword;
  std::function<std::string(const std::string&)> replaceUltraplanKeyword;
  std::function<instructions::hooks::runtime::HookEventGenerator(
    const std::string&,const std::string&,std::stop_token)>
      executeUserPoetrySubmitHooks;
};

event_loop::Deferred<InputResult> processUserInput(const UserInputRequest& request,
  UserInputServices& services);
} // namespace batbox::user_input
