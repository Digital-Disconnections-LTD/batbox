#include "indigenous_yoga/Node.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>
using namespace batbox;
namespace ny=batbox::indigenous_yoga;
namespace {
ny::DimensionInput dimension(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_number())return value.get<double>();
  return std::monostate{};
}
void applyOperation(ny::Node& node,const Json& op){
  const auto method=op[0].get<std::string>();
  const auto num=[&](std::size_t i){return op[i].get<double>();};
  const auto integer=[&](std::size_t i){return op[i].get<int>();};
  if(method=="setWidth")node.setWidth(dimension(op[1]));
  else if(method=="setHeight")node.setHeight(dimension(op[1]));
  else if(method=="setMinWidth")node.setMinWidth(dimension(op[1]));
  else if(method=="setMaxWidth")node.setMaxWidth(dimension(op[1]));
  else if(method=="setMinHeight")node.setMinHeight(dimension(op[1]));
  else if(method=="setMaxHeight")node.setMaxHeight(dimension(op[1]));
  else if(method=="setWidthPercent")node.setWidthPercent(num(1));
  else if(method=="setHeightPercent")node.setHeightPercent(num(1));
  else if(method=="setFlexDirection")node.setFlexDirection(
    ny::FlexDirection(integer(1)));
  else if(method=="setFlexGrow")node.setFlexGrow(num(1));
  else if(method=="setFlexShrink")node.setFlexShrink(num(1));
  else if(method=="setFlexBasis")node.setFlexBasis(dimension(op[1]));
  else if(method=="setFlexBasisPercent")node.setFlexBasisPercent(num(1));
  else if(method=="setFlexWrap")node.setFlexWrap(ny::Wrap(integer(1)));
  else if(method=="setAlignItems")node.setAlignItems(ny::Align(integer(1)));
  else if(method=="setAlignSelf")node.setAlignSelf(ny::Align(integer(1)));
  else if(method=="setAlignContent")node.setAlignContent(ny::Align(integer(1)));
  else if(method=="setJustifyContent")node.setJustifyContent(
    ny::Justify(integer(1)));
  else if(method=="setDisplay")node.setDisplay(ny::Display(integer(1)));
  else if(method=="setOverflow")node.setOverflow(ny::Overflow(integer(1)));
  else if(method=="setPositionType")node.setPositionType(
    ny::PositionType(integer(1)));
  else if(method=="setPosition")node.setPosition(ny::Edge(integer(1)),
    dimension(op[2]));
  else if(method=="setPositionPercent")node.setPositionPercent(
    ny::Edge(integer(1)),num(2));
  else if(method=="setPadding")node.setPadding(ny::Edge(integer(1)),
    dimension(op[2]));
  else if(method=="setPaddingPercent")node.setPaddingPercent(
    ny::Edge(integer(1)),num(2));
  else if(method=="setMargin")node.setMargin(ny::Edge(integer(1)),
    dimension(op[2]));
  else if(method=="setMarginAuto")node.setMarginAuto(ny::Edge(integer(1)));
  else if(method=="setBorder")node.setBorder(ny::Edge(integer(1)),num(2));
  else if(method=="setGap")node.setGap(ny::Gutter(integer(1)),
    dimension(op[2]));
  else if(method=="setGapPercent")node.setGapPercent(
    ny::Gutter(integer(1)),num(2));
  else if(method=="setIsReferenceBaseline")node.setIsReferenceBaseline(
    op[1].get<bool>());
  else throw std::runtime_error("unknown method: "+method);
}
Json run(const Json& spec){
  auto config=ny::createConfig();
  if(spec.contains("scale"))config->setPointScaleFactor(spec["scale"].get<double>());
  std::map<std::string,ny::NodeRef> nodes;
  Json trace=Json::array();
  for(const auto& row:spec.at("nodes")){
    const auto id=row.at("id").get<std::string>();
    auto node=std::make_shared<ny::Node>(config);nodes[id]=node;
    if(row.contains("ops"))for(const auto& op:row.at("ops"))applyOperation(*node,op);
    if(row.contains("measure")){
      const auto width=row["measure"][0].get<double>();
      const auto height=row["measure"][1].get<double>();
      node->setMeasureFunc([&,id,width,height](double w,ny::MeasureMode wm,
          double h,ny::MeasureMode hm){
        trace.push_back(Json::array({id,w,int(wm),h,int(hm)}));
        return ny::Size{width,height};});
    }
  }
  for(const auto& row:spec.at("nodes")){
    if(!row.contains("parent"))continue;
    auto& parent=*nodes.at(row["parent"].get<std::string>());
    parent.insertChild(nodes.at(row["id"].get<std::string>()),
      parent.getChildCount());
  }
  const auto rootId=spec.value("root",spec["nodes"][0]["id"].get<std::string>());
  Json snapshots=Json::array();
  for(const auto& pass:spec.at("passes")){
    if(pass.contains("mutations"))for(const auto& mutation:pass["mutations"]){
      Json op=Json::array();for(std::size_t i=1;i<mutation.size();++i)
        op.push_back(mutation[i]);
      applyOperation(*nodes.at(mutation[0].get<std::string>()),op);
    }
    std::optional<double> width,height;
    if(pass.contains("width"))width=pass["width"].get<double>();
    if(pass.contains("height"))height=pass["height"].get<double>();
    nodes.at(rootId)->calculateLayout(width,height,ny::Direction::LTR);
    Json resultNodes=Json::array();
    for(const auto& row:spec.at("nodes")){
      const auto id=row["id"].get<std::string>();
      const auto& node=*nodes.at(id);
      const auto layout=node.getComputedLayout();
      resultNodes.push_back({{"id",id},{"left",layout.left},
        {"top",layout.top},{"right",layout.right},
        {"bottom",layout.bottom},{"width",layout.width},
        {"height",layout.height},{"dirty",node.isDirty()}});
    }
    const auto counters=ny::getYogaCounters();
    snapshots.push_back({{"nodes",resultNodes},
      {"counters",{{"visited",counters.visited},
        {"measured",counters.measured},{"cacheHits",counters.stashHits},
        {"live",counters.live}}},{"trace",trace}});
  }
  return snapshots;
}
}
int main(){const std::string data((std::istreambuf_iterator<char>(std::cin)),{});
  const auto cases=value::parse(data);Json results=Json::array();
  for(const auto& item:cases)results.push_back(run(item));
  std::cout<<value::stringify(results)<<'\n';}
