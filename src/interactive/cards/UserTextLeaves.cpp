#include "interactive/cards/UserTextLeaves.hpp"
#include "interactive/MessageTags.hpp"

namespace batbox::interactive::cards {

std::optional<BashInputLeaf> bashInputLeaf(const std::string& text,
                                           bool addMargin) {
  const auto input = extractTag(text, "bash-input");
  if (!input || input->empty()) return {};
  return BashInputLeaf{*input, addMargin};
}

BashOutputLeaf bashOutputLeaf(const std::string& text, bool verbose) {
  const auto rawStdout = extractTag(text, "bash-stdout").value_or("");
  const auto stdoutText = extractTag(rawStdout, "persisted-output")
                              .value_or(rawStdout);
  const auto stderrText = extractTag(text, "bash-stderr").value_or("");
  return {stdoutText, stderrText, verbose};
}

std::optional<CommandLeaf> commandLeaf(const std::string& text,
                                       const std::string& commandTag,
                                       bool addMargin) {
  const auto command = extractTag(text, commandTag);
  const auto args = extractTag(text, "command-args");
  const bool skill = extractTag(text, "skill-format") == "true";
  if (!command || command->empty()) return {};
  if (skill) return CommandLeaf{"Skill(" + *command + ")", true, addMargin};
  std::string label = "/" + *command;
  if (args && !args->empty()) label += " " + *args;
  return CommandLeaf{std::move(label), false, addMargin};
}
} // namespace batbox::interactive::cards
