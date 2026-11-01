#pragma once
#include "runtime/JsonValue.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace batbox::history {
struct Reference {
  double id=0;
  std::string match;
  std::size_t index=0; // UTF-16 code-unit offset.
};
std::size_t getPastedTextRefNumLines(const std::string& text);
std::string formatPastedTextRef(double id,double numLines,
  std::optional<std::size_t> characters={});
std::string formatImageRef(double id);
std::vector<Reference> parseReferences(const std::string& input);
std::string expandPastedTextRefs(const std::string& input,const Json& pastedContents);
}
