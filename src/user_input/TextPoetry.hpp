#pragma once
#include "messages/Messages.hpp"
#include <optional>
#include <string>

namespace batbox::user_input {
struct InputResult {
  Json messages=Json::array();
  bool shouldQuery=false;
  std::optional<Json> allowedGadgets;
  std::optional<std::string> model,effort,resultText,nextInput;
  std::optional<bool> submitNextInput;
};

InputResult processTextPoetry(const Json& input,const Json& imageContentBlocks,
  const Json& imagePasteIds,const Json& attachmentMessages,
  const std::optional<std::string>& uuid,
  const std::optional<std::string>& permissionMode,bool isMeta,
  messages::Services& messageServices);
} // namespace batbox::user_input
