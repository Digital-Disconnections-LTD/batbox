#include "terminal_output/OutputRender.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_output {
namespace {
using terminal_screen::typed_array::jsMin;
std::vector<std::string> splitLines(const std::string& text){
  std::vector<std::string> lines;
  std::size_t begin=0;
  while(true){
    const auto end=text.find('\n',begin);
    if(end==std::string::npos){lines.push_back(text.substr(begin));break;}
    lines.push_back(text.substr(begin,end-begin));begin=end+1;
  }
  return lines;
}
bool wrapAt(const std::shared_ptr<std::vector<bool>>& wrap,double index){
  if(!wrap||!std::isfinite(index)||index<0||std::trunc(index)!=index||
      index>=static_cast<double>(wrap->size()))return false;
  return (*wrap)[static_cast<std::size_t>(index)];
}
std::string lineAt(const std::vector<std::string>& lines,double index){
  if(!std::isfinite(index)||index<0||std::trunc(index)!=index||
      index>=static_cast<double>(lines.size()))return {};
  return lines[static_cast<std::size_t>(index)];
}
}
void applyWriteOperation(Output& output,const WriteOperation& operation,
    const ClipRef& clip,const OutputRenderServices& services,
    double& writeCells){
  if(!services.line.stringWidth||!services.sliceAnsi||!services.widestLine)
    throw std::invalid_argument("Output width and ANSI slice imports required");
  const auto& text=operation.text;
  const auto& softWrap=operation.softWrap;
  double x=operation.x,y=operation.y;
  auto lines=splitLines(text);
  double swFrom=0,prevContentEnd=0;
  const auto& width=services.line.stringWidth;
  if(clip){
    const bool clipHorizontally=clip->x1.has_value()&&clip->x2.has_value();
    const bool clipVertically=clip->y1.has_value()&&clip->y2.has_value();
    if(clipHorizontally){
      const auto lineWidth=services.widestLine(text);
      if(x+lineWidth<=*clip->x1||x>=*clip->x2)return;
    }
    if(clipVertically){
      const auto height=static_cast<double>(lines.size());
      if(y+height<=*clip->y1||y>=*clip->y2)return;
    }
    if(clipHorizontally){
      for(auto& line:lines){
        const auto from=x<*clip->x1?*clip->x1-x:0;
        const auto lineWidth=width(line);
        const auto to=x+lineWidth>*clip->x2?*clip->x2-x:lineWidth;
        auto sliced=services.sliceAnsi(line,from,to);
        if(width(sliced)>to-from)
          sliced=services.sliceAnsi(line,from,to-1);
        line=std::move(sliced);
      }
      if(x<*clip->x1)x=*clip->x1;
    }
    if(clipVertically){
      const auto from=y<*clip->y1?*clip->y1-y:0;
      const auto height=static_cast<double>(lines.size());
      const auto to=y+height>*clip->y2?*clip->y2-y:height;
      if(softWrap&&from>0&&wrapAt(softWrap,from))
        prevContentEnd=x+width(lineAt(lines,from-1));
      const auto first=terminal_screen::typed_array::relativeIndex(from,
        lines.size());
      const auto last=terminal_screen::typed_array::relativeIndex(to,
        lines.size());
      if(first<last)lines=std::vector<std::string>(lines.begin()+first,
        lines.begin()+last);
      else lines.clear();
      swFrom=from;
      if(y<*clip->y1)y=*clip->y1;
    }
  }
  double offsetY=0;
  for(const auto& line:lines){
    const auto lineY=y+offsetY;
    if(lineY>=output.height)break;
    const auto contentEnd=writeLineToScreen(*output.screen,line,x,lineY,
      output.width,*output.stylePool,output.charStash,services.line);
    writeCells+=contentEnd-x;
    if(softWrap){
      const bool isSW=wrapAt(softWrap,swFrom+offsetY);
      if(std::isfinite(lineY)&&lineY>=0&&std::trunc(lineY)==lineY&&
          lineY<static_cast<double>(output.screen->softWrap->size()))
        (*output.screen->softWrap)[static_cast<std::size_t>(lineY)]=
          terminal_screen::typed_array::toInt32(isSW?prevContentEnd:0);
      prevContentEnd=contentEnd;
    }
    offsetY++;
  }
}
} // namespace batbox::terminal_output
