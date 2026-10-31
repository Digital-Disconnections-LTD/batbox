#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::interactive::cards_gadget_result {

struct FallbackErrorCard {
  std::string errorText;
  int extraLines = 0;
  std::string shortcut;
  bool showExpand = false;
};

FallbackErrorCard fallbackErrorCard(const Json& result, bool verbose,
    const std::function<std::string()>& shortcutDisplay);

} // namespace batbox::interactive::cards_gadget_result
