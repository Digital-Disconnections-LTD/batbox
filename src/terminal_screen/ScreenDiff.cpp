#include "terminal_screen/DiffRows.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <algorithm>

namespace batbox::terminal_screen {
bool diffEach(const Screen& previous,const Screen& next,
    const DiffCallback& callback){
  const auto previousWidth=previous.width,nextWidth=next.width;
  const auto previousHeight=previous.height,nextHeight=next.height;
  Rectangle region;
  if(previousWidth==0&&previousHeight==0){
    region={0,0,nextWidth,nextHeight};
  }else if(next.damage){
    region=*next.damage;
    if(previous.damage)region=unionRect(region,*previous.damage);
  }else if(previous.damage){
    region=*previous.damage;
  }else{
    region={0,0,0,0};
  }
  if(previousHeight>nextHeight){
    region=unionRect(region,{0,nextHeight,previousWidth,
      previousHeight-nextHeight});
  }
  if(previousWidth>nextWidth){
    region=unionRect(region,{nextWidth,0,
      previousWidth-nextWidth,previousHeight});
  }
  const auto maxHeight=typed_array::jsMax(previousHeight,nextHeight);
  const auto maxWidth=typed_array::jsMax(previousWidth,nextWidth);
  const auto endY=typed_array::jsMin(region.y+region.height,maxHeight);
  const auto endX=typed_array::jsMin(region.x+region.width,maxWidth);
  if(previousWidth==nextWidth)
    return diffSameWidth(previous,next,region.x,endX,region.y,endY,callback);
  return diffDifferentWidth(previous,next,region.x,endX,region.y,endY,callback);
}
std::vector<DiffEntry> diff(const Screen& previous,const Screen& next){
  std::vector<DiffEntry> output;
  diffEach(previous,next,[&](double x,double y,
      const Cell* removed,const Cell* added){
    output.push_back({{x,y},removed?std::optional<Cell>(*removed):
      std::optional<Cell>{},added?std::optional<Cell>(*added):
      std::optional<Cell>{}});
    return false;
  });
  return output;
}
} // namespace batbox::terminal_screen
