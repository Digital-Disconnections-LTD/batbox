#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::bash_gadget {
struct HintProtocol {
  std::string tagName;
  std::function<void(const std::string&)> logDebug;
};
Json extractBatboxHints(const std::string& output,const std::string& command,
                        const HintProtocol& protocol);
}
