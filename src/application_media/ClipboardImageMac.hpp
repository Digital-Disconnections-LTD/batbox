#pragma once
#include <optional>
#include <string>

namespace batbox::application_media::indigenous {
// Implemented with AppKit in ClipboardImageMac.mm. The C++ clipboard adapter
// keeps its AppleScript fallback when the pasteboard cannot provide PNG data.
std::optional<std::string> readMacClipboardPng();
std::optional<std::string> readMacClipboardFilePath();
}
