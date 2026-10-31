#pragma once

#include "interactive/PresentationState.hpp"
#include <string>

namespace batbox::interactive {

void rebuildTranscriptRows(PresentationState& state);
std::optional<std::string> visibleStreamingText(const PresentationState& state);

} // namespace batbox::interactive
