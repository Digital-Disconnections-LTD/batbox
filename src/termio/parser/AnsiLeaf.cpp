#include "termio/parser/AnsiLeaf.hpp"
namespace batbox::termio::parser::ansi {
bool isC0(double byte){return byte<0x20||byte==0x7f;}
}
