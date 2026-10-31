#include "interactive/cards_assistant/AssistantBinding.hpp"

namespace batbox::interactive::cards_assistant {

std::optional<ftxui::Element> projectAssistantTextBlock(
    const Json& block, bool verbose, bool addMargin, bool selected,
    bool shouldShowDot, const AssistantRenderServices& services) {
  const auto found = block.find("text");
  const auto text = found != block.end() && found->is_string() ?
      found->get<std::string>() : std::string{};
  return projectAssistantText(classifyAssistantText(
      text, verbose, addMargin, selected, shouldShowDot,
      services.protocol, services.textImports), services.projection);
}

std::optional<ftxui::Element> projectAssistantThinkingBlock(
    const Json& block, bool addMargin, bool transcriptMode, bool verbose,
    bool hideInTranscript, const ProjectionImports& imports) {
  return projectThinking(thinkingFromBlock(block, addMargin, transcriptMode,
        verbose, hideInTranscript), imports);
}

} // namespace batbox::interactive::cards_assistant
