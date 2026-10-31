#pragma once
#include <functional>
#include <string>
#include <vector>

namespace batbox::terminal_render {
struct MatchPosition {double row=0,col=0,len=0;};
}
namespace batbox::terminal_search {
struct Cell {std::string character;bool skip=false;};
std::u16string lowercase(const std::u16string& text);
std::vector<terminal_render::MatchPosition> scanRows(double width,double height,
  const std::u16string& loweredQuery,const std::function<Cell(double,double)>& readCell);
} // namespace batbox::terminal_search
