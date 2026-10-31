#include "terminal_search/Scan.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/ustring.h>
#include <stdexcept>

namespace batbox::terminal_search {
std::u16string lowercase(const std::u16string& text){
  UErrorCode error=U_ZERO_ERROR;
  const auto count=u_strToLower(nullptr,0,reinterpret_cast<const UChar*>(text.data()),
    static_cast<int32_t>(text.size()),"",&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))throw std::runtime_error("Unicode lowercasing failed");
  std::u16string result(static_cast<std::size_t>(count),u'\0');error=U_ZERO_ERROR;
  const auto produced=u_strToLower(reinterpret_cast<UChar*>(result.data()),count,
    reinterpret_cast<const UChar*>(text.data()),static_cast<int32_t>(text.size()),"",&error);
  if(U_FAILURE(error))throw std::runtime_error("Unicode lowercasing failed");
  result.resize(static_cast<std::size_t>(produced));return result;
}
std::vector<terminal_render::MatchPosition> scanRows(double width,double height,
    const std::u16string& query,const std::function<Cell(double,double)>& readCell){
  if(query.empty())return {};
  std::vector<terminal_render::MatchPosition> positions;
  for(double row=0;row<height;++row){
    std::u16string text;std::vector<double> columns;
    std::vector<std::size_t> unitToCell;
    for(double column=0;column<width;++column){
      const auto cell=readCell(column,row);if(cell.skip)continue;
      const auto lowered=lowercase(value::utf16CodeUnits(cell.character));
      const auto index=columns.size();
      for(std::size_t unit=0;unit<lowered.size();++unit)unitToCell.push_back(index);
      text+=lowered;columns.push_back(column);
    }
    auto position=text.find(query);
    while(position!=std::u16string::npos){
      const auto start=unitToCell.at(position),end=unitToCell.at(position+query.size()-1);
      const auto column=columns.at(start);
      positions.push_back({row,column,columns.at(end)+1-column});
      position=text.find(query,position+query.size());
    }
  }
  return positions;
}
} // namespace batbox::terminal_search
