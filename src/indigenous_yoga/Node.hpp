#pragma once
#include "indigenous_yoga/Types.hpp"

namespace batbox::indigenous_yoga {
class Node;
using NodeRef=std::shared_ptr<Node>;
using DimensionInput=std::variant<double,std::string,std::monostate>;
struct ComputedLayout {
  double left,top,right,bottom,width,height;
};
struct Counters {int visited,measured,stashHits,live;};
Counters getYogaCounters();
void layoutNode(Node& node,double availableWidth,double availableHeight,
  MeasureMode widthMode,MeasureMode heightMode,double ownerWidth,
  double ownerHeight,bool performLayout,bool forceWidth=false,
  bool forceHeight=false);
void roundLayout(Node& node,double scale,double absLeft,double absTop);

class Node:public std::enable_shared_from_this<Node> {
public:
  Style style;
  Layout layout;
  Node* parent=nullptr;
  std::vector<NodeRef> children;
  MeasureFunction measureFunc;
  ConfigRef config;
  bool isDirty_=true,isReferenceBaseline_=false;
  double _flexBasis=0,_mainSize=0,_crossSize=0;
  int _lineIndex=0;
  bool _hasAutoMargin=false,_hasPosition=false,_hasPadding=false,
    _hasBorder=false,_hasMargin=false;
  double _lW=NaN,_lH=NaN,_lOW=NaN,_lOH=NaN;
  MeasureMode _lWM=MeasureMode::Undefined,_lHM=MeasureMode::Undefined;
  bool _lFW=false,_lFH=false;
  double _lOutW=NaN,_lOutH=NaN;
  bool _hasL=false;
  double _mW=NaN,_mH=NaN,_mOW=NaN,_mOH=NaN,
    _mOutW=NaN,_mOutH=NaN;
  MeasureMode _mWM=MeasureMode::Undefined,_mHM=MeasureMode::Undefined;
  bool _hasM=false;
  double _fbBasis=NaN,_fbOwnerW=NaN,_fbOwnerH=NaN,
    _fbAvailMain=NaN,_fbAvailCross=NaN;
  MeasureMode _fbCrossMode=MeasureMode::Undefined;
  int _fbGen=-1;
  std::array<double,32> _cIn{};
  std::array<double,8> _cOut{};
  bool _stashAllocated=false;
  int _cGen=-1,_cN=0,_cWr=0;

  explicit Node(ConfigRef config={});
  void insertChild(NodeRef child,int index);
  void removeChild(const NodeRef& child);
  NodeRef getChild(int index)const;
  int getChildCount()const;
  Node* getParent()const;
  void free();
  void freeRecursive();
  void reset();
  void markDirty();
  bool isDirty()const;
  bool hasNewLayout()const;
  void markLayoutSeen();
  void setMeasureFunc(MeasureFunction callback);
  void unsetMeasureFunc();
  double getComputedLeft()const;
  double getComputedTop()const;
  double getComputedWidth()const;
  double getComputedHeight()const;
  double getComputedRight()const;
  double getComputedBottom()const;
  ComputedLayout getComputedLayout()const;
  double getComputedBorder(Edge edge)const;
  double getComputedPadding(Edge edge)const;
  double getComputedMargin(Edge edge)const;

  void setWidth(DimensionInput value);
  void setWidthPercent(double value);
  void setWidthAuto();
  void setHeight(DimensionInput value);
  void setHeightPercent(double value);
  void setHeightAuto();
  void setMinWidth(DimensionInput value);
  void setMinWidthPercent(double value);
  void setMinHeight(DimensionInput value);
  void setMinHeightPercent(double value);
  void setMaxWidth(DimensionInput value);
  void setMaxWidthPercent(double value);
  void setMaxHeight(DimensionInput value);
  void setMaxHeightPercent(double value);
  void setFlexDirection(FlexDirection direction);
  void setFlexGrow(std::optional<double> value);
  void setFlexShrink(std::optional<double> value);
  void setFlex(std::optional<double> value);
  void setFlexBasis(DimensionInput value);
  void setFlexBasisPercent(double value);
  void setFlexBasisAuto();
  void setFlexWrap(Wrap value);
  void setAlignItems(Align value);
  void setAlignSelf(Align value);
  void setAlignContent(Align value);
  void setJustifyContent(Justify value);
  void setDisplay(Display value);
  Display getDisplay()const;
  void setPositionType(PositionType value);
  void setPosition(Edge edge,DimensionInput value);
  void setPositionPercent(Edge edge,double value);
  void setPositionAuto(Edge edge);
  void setOverflow(Overflow value);
  void setDirection(Direction value);
  void setBoxSizing(BoxSizing);
  void setMargin(Edge edge,DimensionInput value);
  void setMarginPercent(Edge edge,double value);
  void setMarginAuto(Edge edge);
  void setPadding(Edge edge,DimensionInput value);
  void setPaddingPercent(Edge edge,double value);
  void setBorder(Edge edge,std::optional<double> value);
  void setGap(Gutter gutter,DimensionInput value);
  void setGapPercent(Gutter gutter,double value);

  FlexDirection getFlexDirection()const;
  Justify getJustifyContent()const;
  Align getAlignItems()const;
  Align getAlignSelf()const;
  Align getAlignContent()const;
  double getFlexGrow()const;
  double getFlexShrink()const;
  Value getFlexBasis()const;
  Wrap getFlexWrap()const;
  Value getWidth()const;
  Value getHeight()const;
  Overflow getOverflow()const;
  PositionType getPositionType()const;
  Direction getDirection()const;
  void copyStyle(const Node&);
  void setDirtiedFunc();
  void unsetDirtiedFunc();
  void setIsReferenceBaseline(bool value);
  bool isReferenceBaseline()const;
  void setAspectRatio(std::optional<double>);
  double getAspectRatio()const;
  void setAlwaysFormsContainingBlock(bool);
  void calculateLayout(std::optional<double> ownerWidth={},
    std::optional<double> ownerHeight={},
    std::optional<Direction> direction={});
};
void incrementLiveNodes();
void decrementLiveNodes();
void resetCountersForLayout();
int currentGeneration();
void incrementNodesVisited();
void incrementMeasureCalls();
void incrementStashHits();
} // namespace batbox::indigenous_yoga
