#include "terminal_screen/Screen.hpp"
#include <algorithm>

namespace batbox::terminal_screen {
namespace {
std::int32_t word(const Screen& screen,std::int64_t index){
  return screen.cells->word(index).value_or(0);
}
std::int32_t widthBits(const Screen& screen,std::int64_t ci){
  return word(screen,ci+1)&3;
}
void clearCell(Screen& screen,std::int64_t ci){
  screen.cells->setWord(ci,0);
  screen.cells->setWord(ci+1,packWord1(screen.emptyStyleId,0,0));
}
}
void setCellAt(Screen& screen,double x,double y,const Cell& cell){
  if(x<0||y<0||x>=screen.width||y>=screen.height)return;
  const auto ci=wordOffset(y*screen.width+x);
  const auto prevWidth=widthBits(screen,ci);
  const auto cellWidth=static_cast<std::int32_t>(cell.width);
  if(prevWidth==static_cast<std::int32_t>(CellWidth::Wide)&&
      cell.width!=CellWidth::Wide){
    const auto spacerX=x+1;
    if(spacerX<screen.width){
      const auto spacerCI=ci+2;
      if(widthBits(screen,spacerCI)==
          static_cast<std::int32_t>(CellWidth::SpacerTail))
        clearCell(screen,spacerCI);
    }
  }
  double clearedWideX=-1;
  if(prevWidth==static_cast<std::int32_t>(CellWidth::SpacerTail)&&
      cell.width!=CellWidth::SpacerTail){
    if(x>0){
      const auto wideCI=ci-2;
      if(widthBits(screen,wideCI)==
          static_cast<std::int32_t>(CellWidth::Wide)){
        clearCell(screen,wideCI);
        clearedWideX=x-1;
      }
    }
  }
  screen.cells->setWord(ci,screen.charPool->intern(cell.character));
  screen.cells->setWord(ci+1,packWord1(cell.styleId,
    screen.hyperlinkPool->intern(cell.hyperlink),cellWidth));
  const auto minX=clearedWideX>=0?std::min(x,clearedWideX):x;
  auto& damage=screen.damage;
  if(damage){
    const auto right=damage->x+damage->width;
    const auto bottom=damage->y+damage->height;
    if(minX<damage->x){
      damage->width+=damage->x-minX;
      damage->x=minX;
    }else if(x>=right){
      damage->width=x-damage->x+1;
    }
    if(y<damage->y){
      damage->height+=damage->y-y;
      damage->y=y;
    }else if(y>=bottom){
      damage->height=y-damage->y+1;
    }
  }else{
    damage=std::make_shared<Rectangle>(Rectangle{minX,y,x-minX+1,1});
  }
  if(cell.width==CellWidth::Wide){
    const auto spacerX=x+1;
    if(spacerX<screen.width){
      const auto spacerCI=ci+2;
      if(widthBits(screen,spacerCI)==
          static_cast<std::int32_t>(CellWidth::Wide)){
        const auto orphanCI=spacerCI+2;
        if(spacerX+1<screen.width&&widthBits(screen,orphanCI)==
            static_cast<std::int32_t>(CellWidth::SpacerTail))
          clearCell(screen,orphanCI);
      }
      screen.cells->setWord(spacerCI,1);
      screen.cells->setWord(spacerCI+1,packWord1(screen.emptyStyleId,0,
        static_cast<std::int32_t>(CellWidth::SpacerTail)));
      if(damage&&spacerX>=damage->x+damage->width)
        damage->width=spacerX-damage->x+1;
    }
  }
}
void setCellStyleId(Screen& screen,double x,double y,std::int32_t styleId){
  if(x<0||y<0||x>=screen.width||y>=screen.height)return;
  const auto ci=wordOffset(y*screen.width+x);
  const auto word1=word(screen,ci+1);
  const auto width=word1&3;
  if(width==static_cast<std::int32_t>(CellWidth::SpacerTail)||
      width==static_cast<std::int32_t>(CellWidth::SpacerHead))return;
  const auto hid=(static_cast<std::uint32_t>(word1)>>2)&0x7fff;
  screen.cells->setWord(ci+1,packWord1(styleId,hid,width));
  const Rectangle one{x,y,1,1};
  if(screen.damage)
    screen.damage=std::make_shared<Rectangle>(unionRect(*screen.damage,one));
  else screen.damage=std::make_shared<Rectangle>(one);
}
} // namespace batbox::terminal_screen
