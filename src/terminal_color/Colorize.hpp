#pragma once
#include "terminal_color/ChalkIndigenous.hpp"
#include "runtime/JsonValue.hpp"
#include "terminal_render/Border.hpp"
#include "terminal_render/StyledText.hpp"
#include "terminal_render/RenderNode.hpp"
#include <optional>

namespace batbox::text_width {class LineWidthStash;}
namespace batbox::terminal_color {
enum class ColorType {Foreground,Background};
std::string colorize(const std::string& text,
  const std::optional<std::string>& color,ColorType type,
  const ChalkState& chalk);
std::string applyTextStyles(const std::string& text,
  const terminal_render::StylesRef& styles,const ChalkState& chalk);
std::string applyColor(const std::string& text,
  const std::optional<std::string>& color,const ChalkState& chalk);
terminal_render::BorderImports bindBorderColor(
  const ChalkState& chalk,
  std::function<double(const std::string&)> stringWidth);
terminal_render::StyledTextImports bindStyledTextColor(
  const ChalkState& chalk,text_width::WrapServices wrapServices);
terminal_render::RenderNodeImports bindRenderNodeImports(
  const ChalkState& chalk,text_width::WrapServices wrapServices,
  const std::shared_ptr<text_width::LineWidthStash>& lineWidthStash,
  std::function<std::optional<std::string>()> termProgram,
  std::function<bool()> isXtermJs);
} // namespace batbox::terminal_color
