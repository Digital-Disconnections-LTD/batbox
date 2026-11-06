#pragma once
#include "poetryEntries/Sections.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace batbox::poetryEntries {
// poetry/system.md as embedded at build time, comments included.
std::string_view builtInSystemPoetrySource();
inline constexpr const char* kUsingYourToolsSection="using_your_tools";
// Every name {{section:NAME}} accepts, in the order unreferenced sections are
// appended. ant_model_override, scratchpad, frc, numeric_length_anchors,
// token_budget and brief are no longer produced and render nothing; they stay
// accepted so an existing system.md renders as before.
const std::vector<std::string>& systemPoetrySectionNames();
// Line endings become LF, a UTF-8 byte order mark and <!-- --> comments are
// removed (a line holding only a comment goes with its line break), and
// trailing whitespace at the end is dropped.
std::string prepareSystemHead(std::string_view source);
struct PlaceholderScan {
  std::size_t known=0;
  // Unrecognised {{...}} tokens as written, first occurrence order, no repeats.
  std::vector<std::string> unknown;
};
PlaceholderScan scanPlaceholders(std::string_view head);
struct HeadSection {
  std::string name;
  SectionValue value;
  // Dynamic sections go after the global stash boundary.
  bool dynamic=true;
};
// Renders {{section:NAME}} and {{sections}} in a prepared head and appends
// unreferenced sections in the given order. A placeholder alone on its line
// becomes its own block; the caller joins blocks with a blank line and skips
// empty ones. Unknown tokens stay as text.
std::vector<std::string> renderSystemHead(const std::string& head,
  const std::vector<HeadSection>& sections,bool globalStashBoundary);
}
