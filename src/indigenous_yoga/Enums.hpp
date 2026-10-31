#pragma once

namespace batbox::indigenous_yoga {
enum class Align:int {Auto=0,FlexStart=1,Center=2,FlexEnd=3,Stretch=4,
  Baseline=5,SpaceBetween=6,SpaceAround=7,SpaceEvenly=8};
enum class BoxSizing:int {BorderBox=0,ContentBox=1};
enum class Dimension:int {Width=0,Height=1};
enum class Direction:int {Inherit=0,LTR=1,RTL=2};
enum class Display:int {Flex=0,None=1,Contents=2};
enum class Edge:int {Left=0,Top=1,Right=2,Bottom=3,Start=4,End=5,
  Horizontal=6,Vertical=7,All=8};
enum class Errata:int {None=0,StretchFlexBasis=1,
  AbsolutePositionWithoutInsetsExcludesPadding=2,
  AbsolutePercentAgainstInnerSize=4,All=2147483647,Classic=2147483646};
enum class ExperimentalFeature:int {WebFlexBasis=0};
enum class FlexDirection:int {Column=0,ColumnReverse=1,Row=2,RowReverse=3};
enum class Gutter:int {Column=0,Row=1,All=2};
enum class Justify:int {FlexStart=0,Center=1,FlexEnd=2,SpaceBetween=3,
  SpaceAround=4,SpaceEvenly=5};
enum class MeasureMode:int {Undefined=0,Exactly=1,AtMost=2};
enum class Overflow:int {Visible=0,Hidden=1,Scroll=2};
enum class PositionType:int {Static=0,Relative=1,Absolute=2};
enum class Unit:int {Undefined=0,Point=1,Percent=2,Auto=3};
enum class Wrap:int {NoWrap=0,Wrap=1,WrapReverse=2};
} // namespace batbox::indigenous_yoga
