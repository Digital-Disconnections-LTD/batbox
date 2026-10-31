#pragma once

#include "interactive/cards_assistant/AssistantProjection.hpp"

namespace batbox::interactive::cards_assistant {

struct AssistantRenderServices {
  Protocol protocol;
  Imports textImports;
  ProjectionImports projection;
};

std::optional<ftxui::Element> projectAssistantTextBlock(
    const Json& block, bool verbose, bool addMargin, bool selected,
    bool shouldShowDot, const AssistantRenderServices& services);

std::optional<ftxui::Element> projectAssistantThinkingBlock(
    const Json& block, bool addMargin, bool transcriptMode, bool verbose,
    bool hideInTranscript, const ProjectionImports& imports);

} // namespace batbox::interactive::cards_assistant
