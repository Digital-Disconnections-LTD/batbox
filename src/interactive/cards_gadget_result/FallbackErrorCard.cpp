#include "interactive/cards_gadget_result/FallbackErrorCard.hpp"
#include "interactive/MessageTags.hpp"
#include "interactive/cards/BashResultText.hpp"
#include "interactive/cards/OutputLineCard.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::cards_gadget_result {
namespace {
void removeAll(std::string& text, const std::string& needle) {
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos)
    text.erase(pos, needle.size());
}
}

FallbackErrorCard fallbackErrorCard(const Json& result, bool verbose,
    const std::function<std::string()>& shortcutDisplay) {
  if (!shortcutDisplay)
    throw std::logic_error("Transcript shortcut display required");
  // Read the display hook before computing the error.
  const auto shortcut = shortcutDisplay();
  std::string error;
  if (!result.is_string()) {
    error = "Gadget execution failed";
  } else {
    const auto raw = result.get<std::string>();
    const auto extracted = extractTag(raw, "tool_use_error").value_or(raw);
    auto cleaned = cards::removeSandboxViolationTags(extracted);
    removeAll(cleaned, "<error>");
    removeAll(cleaned, "</error>");
    const auto trimmed = value::trim(cleaned);
    if (!verbose && trimmed.find("InputValidationError: ") !=
                    std::string::npos)
      error = "Invalid Gadget parameters";
    else if (trimmed.starts_with("Error: ") ||
             trimmed.starts_with("Cancelled: "))
      error = trimmed;
    else
      error = "Error: " + trimmed;
  }
  const int lines = static_cast<int>(std::count(error.begin(),error.end(),'\n'))+1;
  const int extraLines = lines - 10;
  std::string visible = error;
  if (!verbose) {
    int count = 0;
    std::size_t end = 0;
    while (count < 10 && end < error.size()) {
      if (error[end] == '\n') ++count;
      ++end;
    }
    if (count == 10) visible = error.substr(0,end-1);
  }
  visible = cards::stripUnderlineAnsi(visible);
  return {std::move(visible),extraLines,shortcut,
          !verbose && extraLines > 0};
}

} // namespace batbox::interactive::cards_gadget_result
