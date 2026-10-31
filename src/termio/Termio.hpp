#pragma once
#include "termio/parser/Parser.hpp"
#include "termio/parser/StyleCompare.hpp"

namespace batbox::termio {
using parser::Parser;
using parser::Action;
using parser::Color;
using parser::CursorAction;
using parser::EraseAction;
using parser::Grapheme;
using parser::LinkAction;
using parser::ModeAction;
using parser::ScrollAction;
using parser::TextStyle;
using parser::TitleAction;
using parser::colorsEqual;
using parser::defaultStyle;
using parser::stylesEqual;

// Parser discriminants retain their string values at runtime.
// These aliases keep cursor, color, and underline fields consistent.
using CursorDirection=std::string;
using NamedColor=std::string;
using UnderlineStyle=std::string;
struct TextSegment {
  std::string type="text",text;
  TextStyle style=defaultStyle();
};
}
