#include "terminal_screen/Screen.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <algorithm>

namespace batbox::terminal_screen {
void clearRegion(Screen& screen,double regionX,double regionY,
    double regionWidth,double regionHeight){
  const auto startX=typed_array::jsMax(0.0,regionX);
  const auto startY=typed_array::jsMax(0.0,regionY);
  const auto maxX=typed_array::jsMin(regionX+regionWidth,screen.width);
  const auto maxY=typed_array::jsMin(regionY+regionHeight,screen.height);
  if(startX>=maxX||startY>=maxY)return;
  const auto screenWidth=screen.width;
  const auto rowBase=startY*screenWidth;
  double damageMinX=startX,damageMaxX=maxX;
  if(startX==0&&maxX==screen.width){
    screen.cells->fillZeroJS(rowBase,
      rowBase+(maxY-startY)*screenWidth);
  }else{
    const auto stride=wordOffset(screenWidth);
    const auto rowLen=maxX-startX;
    const auto checkLeft=startX>0;
    const auto checkRight=maxX<screen.width;
    auto leftEdge=wordOffset(rowBase+startX);
    auto rightEdge=wordOffset(rowBase+maxX-1);
    auto fillStart=rowBase+startX;
    for(auto y=startY;y<maxY;y++){
      if(checkLeft&&
          (screen.cells->word(leftEdge+1).value_or(0)&3)==
            static_cast<std::int32_t>(CellWidth::SpacerTail)){
        const auto prevW1=leftEdge-1;
        if((screen.cells->word(prevW1).value_or(0)&3)==
            static_cast<std::int32_t>(CellWidth::Wide)){
          screen.cells->setWord(prevW1-1,0);
          screen.cells->setWord(prevW1,packWord1(screen.emptyStyleId,0,0));
          damageMinX=startX-1;
        }
      }
      if(checkRight&&
          (screen.cells->word(rightEdge+1).value_or(0)&3)==
            static_cast<std::int32_t>(CellWidth::Wide)){
        const auto nextW1=rightEdge+3;
        if((screen.cells->word(nextW1).value_or(0)&3)==
            static_cast<std::int32_t>(CellWidth::SpacerTail)){
          screen.cells->setWord(nextW1-1,0);
          screen.cells->setWord(nextW1,packWord1(screen.emptyStyleId,0,0));
          damageMaxX=maxX+1;
        }
      }
      screen.cells->fillZeroJS(fillStart,fillStart+rowLen);
      leftEdge+=stride;
      rightEdge+=stride;
      fillStart+=screenWidth;
    }
  }
  const Rectangle regionRect{damageMinX,startY,
    damageMaxX-damageMinX,maxY-startY};
  if(screen.damage)
    screen.damage=std::make_shared<Rectangle>(unionRect(*screen.damage,regionRect));
  else screen.damage=std::make_shared<Rectangle>(regionRect);
}
} // namespace batbox::terminal_screen
