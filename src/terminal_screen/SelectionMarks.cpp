#include "terminal_screen/Screen.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <algorithm>

namespace batbox::terminal_screen {
void markNoSelectRegion(Screen& screen,double x,double y,
    double width,double height){
  const auto maxX=typed_array::jsMin(x+width,screen.width);
  const auto maxY=typed_array::jsMin(y+height,screen.height);
  auto& noSel=*screen.noSelect;
  const auto stride=screen.width;
  for(auto row=typed_array::jsMax(0.0,y);row<maxY;row++){
    const auto rowStart=row*stride;
    typed_array::fill(noSel,std::uint8_t{1},
      rowStart+typed_array::jsMax(0.0,x),rowStart+maxX);
  }
}
} // namespace batbox::terminal_screen
