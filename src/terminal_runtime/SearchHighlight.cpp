#include "terminal_runtime/SearchHighlight.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/ustring.h>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace batbox::terminal_runtime {
namespace {
std::u16string lower(const std::u16string& input){
  UErrorCode error=U_ZERO_ERROR;
  const auto count=u_strToLower(nullptr,0,
    reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),"",&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("Unicode lowercasing failed");
  std::u16string result(static_cast<std::size_t>(count),u'\0');
  error=U_ZERO_ERROR;
  const auto produced=u_strToLower(
    reinterpret_cast<UChar*>(result.data()),count,
    reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),"",&error);
  if(U_FAILURE(error))throw std::runtime_error("Unicode lowercasing failed");
  result.resize(static_cast<std::size_t>(produced));
  return result;
}
}
bool applySearchHighlight(terminal_screen::Screen& screen,
    const std::string& query,terminal_screen::StylePool& stylePool){
  if(query.empty())return false;
  const auto lq=lower(value::utf16CodeUnits(query));
  const auto qlen=lq.size();
  const auto width=screen.width;
  const auto height=screen.height;
  const auto& noSelect=*screen.noSelect;
  bool applied=false;
  for(double row=0;row<height;row++){
    const auto rowOff=row*width;
    std::u16string text;
    std::vector<double> colOf;
    std::vector<std::size_t> codeUnitToCell;
    for(double col=0;col<width;col++){
      const auto idx=rowOff+col;
      const auto cell=terminal_screen::cellAtIndex(screen,idx);
      const bool selected=std::isfinite(idx)&&idx>=0&&
        std::trunc(idx)==idx&&idx<static_cast<double>(noSelect.size())&&
        noSelect[static_cast<std::size_t>(idx)]==1;
      if(cell.width==terminal_screen::CellWidth::SpacerTail||
         cell.width==terminal_screen::CellWidth::SpacerHead||selected)continue;
      const auto lc=lower(value::utf16CodeUnits(cell.character));
      const auto cellIdx=colOf.size();
      for(std::size_t i=0;i<lc.size();i++)codeUnitToCell.push_back(cellIdx);
      text+=lc;
      colOf.push_back(col);
    }
    auto pos=text.find(lq);
    while(pos!=std::u16string::npos){
      applied=true;
      const auto startCi=codeUnitToCell.at(pos);
      const auto endCi=codeUnitToCell.at(pos+qlen-1);
      for(auto ci=startCi;ci<=endCi;ci++){
        const auto col=colOf.at(ci);
        const auto cell=terminal_screen::cellAtIndex(screen,rowOff+col);
        terminal_screen::setCellStyleId(screen,col,row,
          stylePool.withInverse(cell.styleId));
      }
      pos=text.find(lq,pos+qlen);
    }
  }
  return applied;
}
} // namespace batbox::terminal_runtime
