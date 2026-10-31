#pragma once
#include "terminal_dom/Node.hpp"
#include "terminal_output/Output.hpp"
#include <array>
#include <functional>
#include <optional>
#include <string>

namespace batbox::terminal_render {
struct BoxStyle {
  std::string topLeft,top,topRight,right,bottomRight,bottom,bottomLeft,left;
};
using EmbeddedBorderText=std::array<std::string,3>;
struct BorderImports {
  std::function<double(const std::string&)> stringWidth;
  std::function<std::string(const std::string&,
    const std::optional<std::string>&)> applyColor;
  std::function<std::string(const std::string&)> dim;
  std::function<std::optional<BoxStyle>(const std::string&)> resolveBox;
};
std::optional<BoxStyle> selectedCliBox(const std::string& name);
EmbeddedBorderText embedTextInBorder(const std::string& borderLine,
  const std::string& text,const std::string& align,double offset,
  const std::string& borderChar,const BorderImports& imports);
std::string styleBorderLine(const std::string& line,
  const std::optional<std::string>& color,bool dim,
  const BorderImports& imports);
void renderBorder(double x,double y,const terminal_dom::DOMNode& node,
  terminal_output::Output& output,const BorderImports& imports);
} // namespace batbox::terminal_render
