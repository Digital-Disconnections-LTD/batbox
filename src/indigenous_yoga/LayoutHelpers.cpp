#include "indigenous_yoga/LayoutInternal.hpp"
#include <algorithm>

namespace batbox::indigenous_yoga {
double childMarginForAxis(const Node& child,FlexDirection axis,
    double ownerWidth){
  if(!child._hasMargin)return 0;
  const auto leading=resolveEdge(child.style.margin,
    leadingEdge(axis),ownerWidth);
  const auto trailing=resolveEdge(child.style.margin,
    trailingEdge(axis),ownerWidth);
  return leading+trailing;
}
double resolveGap(const Style& style,Gutter gutter,double ownerSize){
  auto value=style.gap[int(gutter)];
  if(value.unit==Unit::Undefined)value=style.gap[int(Gutter::All)];
  const auto resolved=resolveValue(value,ownerSize);
  return isDefined(resolved)?std::max(0.0,resolved):0;
}
double boundAxis(const Style& style,bool isWidth,double value,
    double ownerWidth,double ownerHeight){
  const auto min=isWidth?style.minWidth:style.minHeight;
  const auto max=isWidth?style.maxWidth:style.maxHeight;
  if(min.unit==Unit::Undefined&&max.unit==Unit::Undefined)return value;
  const auto owner=isWidth?ownerWidth:ownerHeight;
  auto result=value;
  if(max.unit==Unit::Point){
    if(result>max.value)result=max.value;
  }else if(max.unit==Unit::Percent){
    const auto measured=max.value*owner/100;
    if(measured==measured&&result>measured)result=measured;
  }
  if(min.unit==Unit::Point){
    if(result<min.value)result=min.value;
  }else if(min.unit==Unit::Percent){
    const auto measured=min.value*owner/100;
    if(measured==measured&&result<measured)result=measured;
  }
  return result;
}
void zeroLayoutRecursive(Node& node){
  for(const auto& ref:node.children){
    auto& child=*ref;
    child.layout.left=0;child.layout.top=0;
    child.layout.width=0;child.layout.height=0;
    child.isDirty_=true;child._hasL=false;child._hasM=false;
    zeroLayoutRecursive(child);
  }
}
void collectLayoutChildren(Node& node,NodeList& flow,NodeList& absolute){
  for(const auto& ref:node.children){
    auto& child=*ref;
    const auto display=child.style.display;
    if(display==Display::None){
      child.layout.left=0;child.layout.top=0;
      child.layout.width=0;child.layout.height=0;
      zeroLayoutRecursive(child);
    }else if(display==Display::Contents){
      child.layout.left=0;child.layout.top=0;
      child.layout.width=0;child.layout.height=0;
      collectLayoutChildren(child,flow,absolute);
    }else if(child.style.positionType==PositionType::Absolute){
      absolute.push_back(&child);
    }else flow.push_back(&child);
  }
}
bool hasMeasureFuncInSubtree(const Node& node){
  if(node.measureFunc)return true;
  for(const auto& child:node.children)
    if(hasMeasureFuncInSubtree(*child))return true;
  return false;
}
bool isStretchAlign(const Node& child){
  const auto* parent=child.parent;
  if(!parent)return false;
  const auto align=child.style.alignSelf==Align::Auto?
    parent->style.alignItems:child.style.alignSelf;
  return align==Align::Stretch;
}
Align resolveChildAlign(const Node& parent,const Node& child){
  return child.style.alignSelf==Align::Auto?
    parent.style.alignItems:child.style.alignSelf;
}
double calculateBaseline(const Node& node){
  const Node* baseline=nullptr;
  for(const auto& child:node.children){
    if(child->_lineIndex>0)break;
    if(child->style.positionType==PositionType::Absolute)continue;
    if(child->style.display==Display::None)continue;
    if(resolveChildAlign(node,*child)==Align::Baseline||
        child->isReferenceBaseline_){baseline=child.get();break;}
    if(!baseline)baseline=child.get();
  }
  if(!baseline)return node.layout.height;
  return calculateBaseline(*baseline)+baseline->layout.top;
}
bool isBaselineLayout(const Node& node,const NodeList& flow){
  if(!isRow(node.style.flexDirection))return false;
  if(node.style.alignItems==Align::Baseline)return true;
  for(const auto* child:flow)
    if(child->style.alignSelf==Align::Baseline)return true;
  return false;
}
} // namespace batbox::indigenous_yoga
