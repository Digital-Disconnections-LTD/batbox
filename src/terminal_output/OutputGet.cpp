#include "terminal_output/OutputRender.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_output {
namespace {
using terminal_screen::typed_array::jsMax;
using terminal_screen::typed_array::jsMin;
double min4(double a,double b,double c,double d){
  return jsMin(jsMin(jsMin(a,b),c),d);
}
std::string fixedOne(double value){
  // The percentage is nonnegative and at most 100. The extended precision
  // multiplication retains the exact binary64 value before decimal rounding.
  const auto tenths=static_cast<unsigned long long>(
    std::floor(static_cast<long double>(value)*10+0.5L));
  return std::to_string(tenths/10)+"."+std::to_string(tenths%10);
}
}
ScreenRef Output::get(const OutputRenderServices& services){
  auto target=screen;
  const auto screenWidth=width,screenHeight=height;
  double blitCells=0,writeCells=0;
  std::vector<Rectangle> absoluteClears;
  for(const auto& operation:operations){
    const auto clear=std::get_if<ClearOperation>(&operation);
    if(!clear)continue;
    const auto& region=*clear->region;
    const auto startX=jsMax(0,region.x);
    const auto startY=jsMax(0,region.y);
    const auto maxX=jsMin(region.x+region.width,screenWidth);
    const auto maxY=jsMin(region.y+region.height,screenHeight);
    if(startX>=maxX||startY>=maxY)continue;
    const Rectangle rect{startX,startY,maxX-startX,maxY-startY};
    if(target->damage)
      target->damage=std::make_shared<Rectangle>(
        terminal_screen::unionRect(*target->damage,rect));
    else target->damage=std::make_shared<Rectangle>(rect);
    if(clear->fromAbsolute.value_or(false))absoluteClears.push_back(rect);
  }
  std::vector<ClipRef> clips;
  for(const auto& operation:operations){
    if(std::holds_alternative<ClearOperation>(operation))continue;
    if(const auto clip=std::get_if<ClipOperation>(&operation)){
      clips.push_back(intersectClip(clips.empty()?ClipRef{}:clips.back(),
        clip->clip));
      continue;
    }
    if(std::holds_alternative<UnclipOperation>(operation)){
      if(!clips.empty())clips.pop_back();
      continue;
    }
    if(const auto blit=std::get_if<BlitOperation>(&operation)){
      const auto& src=*blit->src;
      const auto regionX=blit->x,regionY=blit->y;
      const auto regionWidth=blit->width,regionHeight=blit->height;
      const auto clip=clips.empty()?ClipRef{}:clips.back();
      const auto startX=jsMax(regionX,clip&&clip->x1?*clip->x1:0);
      const auto startY=jsMax(regionY,clip&&clip->y1?*clip->y1:0);
      const auto inf=std::numeric_limits<double>::infinity();
      const auto maxY=min4(regionY+regionHeight,screenHeight,src.height,
        clip&&clip->y2?*clip->y2:inf);
      const auto maxX=min4(regionX+regionWidth,screenWidth,src.width,
        clip&&clip->x2?*clip->x2:inf);
      if(startX>=maxX||startY>=maxY)continue;
      if(absoluteClears.empty()){
        terminal_screen::blitRegion(*target,src,startX,startY,maxX,maxY);
        blitCells+=(maxY-startY)*(maxX-startX);
        continue;
      }
      double rowStart=startY;
      for(double row=startY;row<=maxY;row++){
        bool excluded=false;
        if(row<maxY){
          for(const auto& rect:absoluteClears){
            if(row>=rect.y&&row<rect.y+rect.height&&startX>=rect.x&&
                maxX<=rect.x+rect.width){excluded=true;break;}
          }
        }
        if(excluded||row==maxY){
          if(row>rowStart){
            terminal_screen::blitRegion(*target,src,startX,rowStart,maxX,row);
            blitCells+=(row-rowStart)*(maxX-startX);
          }
          rowStart=row+1;
        }
      }
      continue;
    }
    if(const auto shift=std::get_if<ShiftOperation>(&operation)){
      terminal_screen::shiftRows(*target,shift->top,shift->bottom,shift->n);
      continue;
    }
    if(const auto write=std::get_if<WriteOperation>(&operation)){
      applyWriteOperation(*this,*write,
        clips.empty()?ClipRef{}:clips.back(),services,writeCells);
      continue;
    }
  }
  for(const auto& operation:operations){
    if(const auto noSelect=std::get_if<NoSelectOperation>(&operation)){
      const auto& r=*noSelect->region;
      terminal_screen::markNoSelectRegion(*target,r.x,r.y,r.width,r.height);
    }
  }
  const auto totalCells=blitCells+writeCells;
  if(totalCells>1000&&writeCells>blitCells){
    if(!services.logForDebugging)
      throw std::invalid_argument("Output debug diagnostic import required");
    services.logForDebugging("High write ratio: blit="+
      value::numberToString(blitCells)+", write="+value::numberToString(writeCells)+
      " ("+fixedOne((writeCells/totalCells)*100)+"% writes), screen="+
      value::numberToString(screenHeight)+"x"+value::numberToString(screenWidth));
  }
  return target;
}
} // namespace batbox::terminal_output
