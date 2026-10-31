#pragma once
#include "user_input/ProcessUserInput.hpp"
#include <memory>

namespace batbox::user_input::internal {
struct InputState {
  UserInputRequest request;
  UserInputServices& services;
  Json appState;
  std::optional<std::string> permissionMode,inputString;
  Json normalizedInput,preceding=Json::array(),processed=Json::array();
  Json imageContents=Json::array(),imagePasteIds=Json::array();
  Json imageBlocks=Json::array(),attachments=Json::array();
  Json storedPaths=Json::object();
  std::vector<std::string> imageMetadata;
  std::size_t blockIndex=0;
  event_loop::Deferred<InputResult> result;
  InputState(UserInputRequest request,UserInputServices& services);
};
void prepareInput(const std::shared_ptr<InputState>& state);
void processPasted(const std::shared_ptr<InputState>& state);
void loadAttachmentsAndRoute(const std::shared_ptr<InputState>& state);
void finishBase(const std::shared_ptr<InputState>& state,InputResult result);
void runHooks(const std::shared_ptr<InputState>& state,InputResult result);
void fail(const std::shared_ptr<InputState>& state,std::exception_ptr error);
std::string text(const Json& value);
void checkpoint(InputState& state,const std::string& name);
void appendMetadata(const std::shared_ptr<InputState>& state,InputResult& result);
} // namespace batbox::user_input::internal
