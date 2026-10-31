#include "indigenous_yoga/Node.hpp"
#include <cmath>

namespace batbox::indigenous_yoga {
namespace {
bool isWholeNumber(double value){
  const auto fraction=value-std::floor(value);
  return fraction<0.0001||fraction>0.9999;
}
double roundValue(double value,double scale,bool forceCeil,bool forceFloor){
  auto scaled=value*scale;
  auto fraction=scaled-std::floor(scaled);
  if(fraction<0)fraction+=1;
  if(fraction<0.0001)scaled=std::floor(scaled);
  else if(fraction>0.9999)scaled=std::ceil(scaled);
  else if(forceCeil)scaled=std::ceil(scaled);
  else if(forceFloor)scaled=std::floor(scaled);
  else scaled=std::floor(scaled)+(fraction>=0.4999?1:0);
  return scaled/scale;
}
}
void roundLayout(Node& node,double scale,double absLeft,double absTop){
  if(scale==0)return;
  auto& layout=node.layout;
  const auto nodeLeft=layout.left,nodeTop=layout.top;
  const auto nodeWidth=layout.width,nodeHeight=layout.height;
  const auto absNodeLeft=absLeft+nodeLeft;
  const auto absNodeTop=absTop+nodeTop;
  const bool text=bool(node.measureFunc);
  layout.left=roundValue(nodeLeft,scale,false,text);
  layout.top=roundValue(nodeTop,scale,false,text);
  const auto absRight=absNodeLeft+nodeWidth;
  const auto absBottom=absNodeTop+nodeHeight;
  const bool fracW=!isWholeNumber(nodeWidth*scale);
  const bool fracH=!isWholeNumber(nodeHeight*scale);
  layout.width=roundValue(absRight,scale,text&&fracW,text&&!fracW)-
    roundValue(absNodeLeft,scale,false,text);
  layout.height=roundValue(absBottom,scale,text&&fracH,text&&!fracH)-
    roundValue(absNodeTop,scale,false,text);
  for(const auto& child:node.children)
    roundLayout(*child,scale,absNodeLeft,absNodeTop);
}
} // namespace batbox::indigenous_yoga
