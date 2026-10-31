#include "indigenous_yoga/Types.hpp"

namespace batbox::indigenous_yoga {
bool isRow(FlexDirection dir){
  return dir==FlexDirection::Row||dir==FlexDirection::RowReverse;
}
bool isReverse(FlexDirection dir){
  return dir==FlexDirection::RowReverse||dir==FlexDirection::ColumnReverse;
}
FlexDirection crossAxis(FlexDirection dir){
  return isRow(dir)?FlexDirection::Column:FlexDirection::Row;
}
int leadingEdge(FlexDirection dir){
  switch(dir){
    case FlexDirection::Row:return int(Edge::Left);
    case FlexDirection::RowReverse:return int(Edge::Right);
    case FlexDirection::Column:return int(Edge::Top);
    case FlexDirection::ColumnReverse:return int(Edge::Bottom);
  }
  return int(Edge::Left);
}
int trailingEdge(FlexDirection dir){
  switch(dir){
    case FlexDirection::Row:return int(Edge::Right);
    case FlexDirection::RowReverse:return int(Edge::Left);
    case FlexDirection::Column:return int(Edge::Bottom);
    case FlexDirection::ColumnReverse:return int(Edge::Top);
  }
  return int(Edge::Right);
}
} // namespace batbox::indigenous_yoga
