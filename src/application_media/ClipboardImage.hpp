#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>

namespace batbox::application_media::indigenous {
// Clipboard process and platform API boundary. Returns null when the clipboard
// has no supported image, matching the optional return contract.
Json getImageFromClipboard();
// Full path from clipboard text, used when a terminal pastes only the image
// filename (for example VS Code's integrated terminal on macOS).
std::optional<std::string> getImagePathFromClipboard();
} // namespace batbox::application_media::indigenous
