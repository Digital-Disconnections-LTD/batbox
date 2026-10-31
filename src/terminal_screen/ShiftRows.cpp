#include "terminal_screen/Screen.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::terminal_screen {
void shiftRows(Screen& screen,double top,double bottom,double n){
  if(n==0||top<0||bottom>=screen.height||top>bottom)return;
  const auto w=screen.width;
  const auto absN=std::abs(n);
  auto& cells=*screen.cells;
  auto& noSel=*screen.noSelect;
  auto& wrap=*screen.softWrap;
  if(absN>bottom-top){
    cells.fillZeroJS(top*w,(bottom+1)*w);
    typed_array::fill(noSel,std::uint8_t{0},top*w,(bottom+1)*w);
    typed_array::fill(wrap,std::int32_t{0},top,bottom+1);
    return;
  }
  if(n>0){
    cells.copyWithinJS(top*w,(top+n)*w,(bottom+1)*w);
    typed_array::copyWithin(noSel,top*w,(top+n)*w,(bottom+1)*w);
    typed_array::copyWithin(wrap,top,top+n,bottom+1);
    cells.fillZeroJS((bottom-n+1)*w,(bottom+1)*w);
    typed_array::fill(noSel,std::uint8_t{0},
      (bottom-n+1)*w,(bottom+1)*w);
    typed_array::fill(wrap,std::int32_t{0},bottom-n+1,bottom+1);
  }else{
    cells.copyWithinJS((top-n)*w,top*w,(bottom+n+1)*w);
    typed_array::copyWithin(noSel,(top-n)*w,top*w,(bottom+n+1)*w);
    typed_array::copyWithin(wrap,top-n,top,bottom+n+1);
    cells.fillZeroJS(top*w,(top-n)*w);
    typed_array::fill(noSel,std::uint8_t{0},top*w,(top-n)*w);
    typed_array::fill(wrap,std::int32_t{0},top,top-n);
  }
}
} // namespace batbox::terminal_screen
