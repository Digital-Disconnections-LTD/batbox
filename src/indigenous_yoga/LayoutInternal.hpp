#pragma once
#include "indigenous_yoga/Node.hpp"

namespace batbox::indigenous_yoga {
using NodeList=std::vector<Node*>;
void stashWrite(Node& node,double availableWidth,double availableHeight,
  MeasureMode widthMode,MeasureMode heightMode,double ownerWidth,
  double ownerHeight,bool forceWidth,bool forceHeight,bool wasDirty);
void commitStashOutputs(Node& node,bool performLayout);
double boundAxis(const Style& style,bool isWidth,double value,
  double ownerWidth,double ownerHeight);
double childMarginForAxis(const Node& child,FlexDirection axis,
  double ownerWidth);
double resolveGap(const Style& style,Gutter gutter,double ownerSize);
void zeroLayoutRecursive(Node& node);
void collectLayoutChildren(Node& node,NodeList& flow,NodeList& absolute);
bool hasMeasureFuncInSubtree(const Node& node);
bool isStretchAlign(const Node& child);
Align resolveChildAlign(const Node& parent,const Node& child);
double calculateBaseline(const Node& node);
bool isBaselineLayout(const Node& node,const NodeList& flow);
double computeFlexBasis(Node& child,FlexDirection mainAxis,
  double availableMain,double availableCross,MeasureMode crossMode,
  double ownerWidth,double ownerHeight);
void resolveFlexibleLengths(const NodeList& children,
  double availableInnerMain,double totalFlexBasis,bool mainRow,
  double ownerWidth,double ownerHeight);
void layoutAbsoluteChild(Node& parent,Node& child,double parentWidth,
  double parentHeight,const Edges& padding,const Edges& border);
struct LayoutFrame {
  Node& node;
  double availableWidth,availableHeight,ownerWidth,ownerHeight;
  MeasureMode widthMode,heightMode;
  bool performLayout,forceWidth,forceHeight,wasDirty;
  double width,height;
  MeasureMode wMode,hMode;
  double paddingBorderWidth,paddingBorderHeight;
};
void layoutContainer(LayoutFrame frame);
struct PositionFrame {
  Node& node;
  const std::vector<NodeList>& lines;
  const NodeList& absoluteChildren;
  std::vector<double>& lineConsumedMain;
  std::vector<double>& lineCrossSizes;
  const std::vector<double>& lineMaxAscent;
  double totalLinesCross,gapMain,gapCross,mainPadBorder,crossPadBorder;
  double ownerW,ownerH;
  bool mainRow,wrap,baseline;
  MeasureMode crossMode;
};
void positionContainer(PositionFrame frame);
} // namespace batbox::indigenous_yoga
