#pragma once

#include "interactive/markdown/Renderer.hpp"
#include "text_width/StringWidth.hpp"
#include <memory>

namespace batbox::interactive::markdown {

struct SyntaxHighlighter {
  std::function<bool(const std::string&)> supportsLanguage;
  std::function<std::string(const std::string&,const std::string&)> highlight;
  std::function<void(const std::string&)> debug;
};
struct SemanticAccents {
  std::string listMarkerSgr,strongLeadInSgr;
};

std::shared_ptr<Renderer> makeIndigenousRenderer(
    std::shared_ptr<text_width::StringWidth> width,
    int terminalColumns,bool colors,bool hyperlinks,
    std::string permissionSgr,
    std::optional<SyntaxHighlighter> highlighter={},
    std::optional<SemanticAccents> accents={});

} // namespace batbox::interactive::markdown
