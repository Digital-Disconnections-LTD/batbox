#pragma once
#include "terminal_dom/Node.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::terminal_render {
using StylesRef=std::shared_ptr<Json>;
struct StyledSegment {
  std::string text;
  StylesRef styles;
  std::optional<std::string> hyperlink;
};
using StyledSegments=std::vector<StyledSegment>;
using StyledSegmentsRef=std::shared_ptr<StyledSegments>;
struct StyledTextImports {
  std::function<std::string(const std::string&,const StylesRef&)>
    applyTextStyles;
  std::function<std::string(const std::string&,double,const std::string&)>
    wrapText;
  std::function<std::string(const std::string&,double)> indentString;
};
StyledSegmentsRef squashTextNodesToSegments(
  const terminal_dom::DOMElement& node,StylesRef inheritedStyles={},
  std::optional<std::string> inheritedHyperlink={},StyledSegmentsRef out={});
std::vector<std::size_t> buildCharToSegmentMap(const StyledSegments& segments);
std::string wrapWithOsc8Link(const std::string& text,const std::string& url);
std::string applyStylesToWrappedText(const std::string& wrappedPlain,
  const StyledSegments& segments,const std::vector<std::size_t>& charToSegment,
  const std::string& originalPlain,bool trimEnabled,
  const StyledTextImports& imports);
struct WrappedText {
  std::string wrapped;
  std::shared_ptr<std::vector<bool>> softWrap;
};
WrappedText wrapWithSoftWrap(const std::string& plainText,double maxWidth,
  const std::string& textWrap,const StyledTextImports& imports);
std::string applyPaddingToText(const terminal_dom::DOMElement& node,
  std::string text,const std::shared_ptr<std::vector<bool>>& softWrap,
  const StyledTextImports& imports);
} // namespace batbox::terminal_render

namespace batbox::text_width {struct WrapServices;}
namespace batbox::terminal_render {
StyledTextImports makeIndigenousStyledTextImports(
  std::function<std::string(const std::string&,const StylesRef&)> applyTextStyles,
  text_width::WrapServices wrapServices);
}
