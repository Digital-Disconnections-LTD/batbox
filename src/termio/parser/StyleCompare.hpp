#pragma once
#include "termio/parser/Types.hpp"

namespace batbox::termio::parser {
bool colorsEqual(const Color& a,const Color& b);
bool stylesEqual(const TextStyle& a,const TextStyle& b);
}
