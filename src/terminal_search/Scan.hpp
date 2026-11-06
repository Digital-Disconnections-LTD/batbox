#pragma once
#include <functional>
#include <string>
#include <vector>

namespace batbox::terminal_search {
struct MatchPosition {double row=0,col=0,len=0;};
struct Cell {std::string character;bool skip=false;};
std::u16string lowercase(const std::u16string& text);
std::vector<MatchPosition> scanRows(double width,double height,
  const std::u16string& loweredQuery,const std::function<Cell(double,double)>& readCell);
} // namespace batbox::terminal_search
