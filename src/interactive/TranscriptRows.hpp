#pragma once

#include "interactive/PresentationState.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace batbox::interactive {

void rebuildTranscriptRows(PresentationState& state);
// The prompt glyph of a queued row and its labels, longest first.
std::string queuedInputMarker(const QueuedInput& input);
std::vector<std::string> queuedInputLabels(const QueuedInput& input);
// The same text, viewed in place.
std::optional<std::string_view> visibleStreamingTextView(const PresentationState& state);

} // namespace batbox::interactive
