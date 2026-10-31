#pragma once
#include "interactive/QueryTurn.hpp"
#include "runtime/OrderedStringSet.hpp"

namespace batbox::interactive::lifecycle {
std::optional<std::string> extractCliName(const std::optional<std::string>& command);
runtime::OrderedStringSet extractBashGadgetsFromMessages(
  const TurnMessagesRef& messages,std::size_t first=0);
} // namespace batbox::interactive::lifecycle
