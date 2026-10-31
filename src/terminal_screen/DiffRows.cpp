#include "terminal_screen/DiffRows.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <algorithm>

namespace batbox::terminal_screen {
namespace {
double findNextDiff(const PackedCells& a,const PackedCells& b,
    std::int64_t w0,double count){
  for(double i=0;i<count;i++,w0+=2){
    const auto w1=typed_array::toInt32(static_cast<double>(w0))|1;
    if(a.word(w0)!=b.word(w0)||a.word(w1)!=b.word(w1))return i;
  }
  return count;
}
bool diffRowBoth(const Screen& previous,const Screen& next,
    std::int64_t ci,double y,double startX,double endX,
    Cell& previousCell,Cell& nextCell,const DiffCallback& callback){
  auto x=startX;
  while(x<endX){
    const auto skip=findNextDiff(*previous.cells,*next.cells,ci,endX-x);
    x+=skip;
    ci+=wordOffset(skip);
    if(x>=endX)break;
    cellAtCI(previous,ci,previousCell);
    cellAtCI(next,ci,nextCell);
    if(callback(x,y,&previousCell,&nextCell))return true;
    x++;ci+=2;
  }
  return false;
}
bool diffRowRemoved(const Screen& previous,std::int64_t ci,double y,
    double startX,double endX,Cell& previousCell,
    const DiffCallback& callback){
  for(auto x=startX;x<endX;x++,ci+=2){
    cellAtCI(previous,ci,previousCell);
    if(callback(x,y,&previousCell,nullptr))return true;
  }
  return false;
}
bool diffRowAdded(const Screen& next,std::int64_t ci,double y,
    double startX,double endX,Cell& nextCell,
    const DiffCallback& callback){
  for(auto x=startX;x<endX;x++,ci+=2){
    if(next.cells->word(ci)==std::optional<std::int32_t>{0}&&
        next.cells->word(
          typed_array::toInt32(static_cast<double>(ci))|1)==
          std::optional<std::int32_t>{0})
      continue;
    cellAtCI(next,ci,nextCell);
    if(callback(x,y,nullptr,&nextCell))return true;
  }
  return false;
}
}
bool diffSameWidth(const Screen& previous,const Screen& next,
    double startX,double endX,double startY,double endY,
    const DiffCallback& callback){
  const auto width=previous.width;
  const auto previousHeight=previous.height;
  const auto nextHeight=next.height;
  const auto stride=wordOffset(width);
  Cell previousCell,nextCell;
  const auto rowEndX=typed_array::jsMin(endX,width);
  auto rowCI=wordOffset(startY*width+startX);
  for(auto y=startY;y<endY;y++){
    const auto previousIn=y<previousHeight;
    const auto nextIn=y<nextHeight;
    if(previousIn&&nextIn){
      if(diffRowBoth(previous,next,rowCI,y,startX,rowEndX,
          previousCell,nextCell,callback))return true;
    }else if(previousIn){
      if(diffRowRemoved(previous,rowCI,y,startX,rowEndX,
          previousCell,callback))return true;
    }else if(nextIn){
      if(diffRowAdded(next,rowCI,y,startX,rowEndX,
          nextCell,callback))return true;
    }
    rowCI+=stride;
  }
  return false;
}
bool diffDifferentWidth(const Screen& previous,const Screen& next,
    double startX,double endX,double startY,double endY,
    const DiffCallback& callback){
  const auto previousWidth=previous.width;
  const auto nextWidth=next.width;
  Cell previousCell,nextCell;
  const auto previousStride=wordOffset(previousWidth);
  const auto nextStride=wordOffset(nextWidth);
  auto previousRowCI=wordOffset(startY*previousWidth+startX);
  auto nextRowCI=wordOffset(startY*nextWidth+startX);
  for(auto y=startY;y<endY;y++){
    const auto previousIn=y<previous.height;
    const auto nextIn=y<next.height;
    const auto previousEndX=previousIn?
      typed_array::jsMin(endX,previousWidth):startX;
    const auto nextEndX=nextIn?typed_array::jsMin(endX,nextWidth):startX;
    const auto bothEndX=typed_array::jsMin(previousEndX,nextEndX);
    auto previousCI=previousRowCI,nextCI=nextRowCI;
    for(auto x=startX;x<bothEndX;x++){
      if(previous.cells->word(previousCI)==next.cells->word(nextCI)&&
          previous.cells->word(previousCI+1)==next.cells->word(nextCI+1)){
        previousCI+=2;nextCI+=2;continue;
      }
      cellAtCI(previous,previousCI,previousCell);
      cellAtCI(next,nextCI,nextCell);
      previousCI+=2;nextCI+=2;
      if(callback(x,y,&previousCell,&nextCell))return true;
    }
    if(previousEndX>bothEndX){
      previousCI=previousRowCI+wordOffset(bothEndX-startX);
      for(auto x=bothEndX;x<previousEndX;x++){
        cellAtCI(previous,previousCI,previousCell);
        previousCI+=2;
        if(callback(x,y,&previousCell,nullptr))return true;
      }
    }
    if(nextEndX>bothEndX){
      nextCI=nextRowCI+wordOffset(bothEndX-startX);
      for(auto x=bothEndX;x<nextEndX;x++){
        if(next.cells->word(nextCI)==std::optional<std::int32_t>{0}&&
            next.cells->word(
              typed_array::toInt32(static_cast<double>(nextCI))|1)==
              std::optional<std::int32_t>{0}){
          nextCI+=2;continue;
        }
        cellAtCI(next,nextCI,nextCell);
        nextCI+=2;
        if(callback(x,y,nullptr,&nextCell))return true;
      }
    }
    previousRowCI+=previousStride;
    nextRowCI+=nextStride;
  }
  return false;
}
} // namespace batbox::terminal_screen
