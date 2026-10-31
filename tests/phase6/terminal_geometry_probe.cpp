#include "terminal_screen/Geometry.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>

using namespace batbox;
namespace g=batbox::terminal_screen;
namespace {
Json geometryJson(const g::Edges& e){
  Json result={{"top",e.top},{"right",e.right},{"bottom",e.bottom}};
  if(e.left)result["left"]=*e.left;
  return result;
}
Json geometryJson(const g::Rectangle& r){
  return Json{{"x",r.x},{"y",r.y},{"width",r.width},{"height",r.height}};
}
}
int main(){
  Json rows=Json::array();
  auto add=[&](const char* kind,Json result){
    rows.push_back(Json{{"kind",kind},{"value",std::move(result)}});};
  add("edges1",geometryJson(g::edges(3)));
  add("edges2",geometryJson(g::edges(2,5)));
  add("edges4",geometryJson(g::edges(1,2,3,4)));
  add("edges3",geometryJson(g::edges(1,2,3)));
  add("add",geometryJson(g::addEdges(g::edges(1,2,3,4),g::edges(5,6,7,8))));
  add("addUndefined",geometryJson(g::addEdges(g::edges(1,2,3),g::edges(1))));
  add("zero",geometryJson(g::ZERO_EDGES));
  add("resolveEmpty",geometryJson(g::resolveEdges()));
  g::PartialEdges partial;partial.top=4;partial.left=7;
  add("resolvePartial",geometryJson(g::resolveEdges(partial)));
  add("union",geometryJson(g::unionRect({2,3,4,5},{-2,1,2,8})));
  add("unionNegative",geometryJson(g::unionRect({2,3,-1,2},{5,4,2,-3})));
  add("clampInside",geometryJson(g::clampRect({2,3,4,5},{10,10})));
  add("clampOutside",geometryJson(g::clampRect({20,-4,3,2},{10,10})));
  add("clampZero",geometryJson(g::clampRect({0,0,2,2},{0,0})));
  add("withinYes",g::withinBounds({5,4},{4,3}));
  add("withinX",g::withinBounds({5,4},{5,3}));
  add("withinNegative",g::withinBounds({5,4},{-1,0}));
  add("clampLow",g::clamp(-4,0,10));
  add("clampHigh",g::clamp(22,0,10));
  add("clampNoBounds",g::clamp(7));
  add("clampNanBound",g::clamp(7,std::numeric_limits<double>::quiet_NaN(),5));
  std::cout<<rows.dump()<<'\n';
}
