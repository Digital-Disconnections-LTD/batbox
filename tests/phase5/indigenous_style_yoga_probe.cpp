#include "terminal_dom/style/Style.hpp"
#include "terminal_dom/YogaLayoutNode.hpp"
#include <iostream>
#include <iterator>

using namespace batbox;
namespace {
Json show(const terminal_dom::FullLayoutNode& node){
  return {{"left",node.getComputedLeft()},
    {"top",node.getComputedTop()},
    {"width",node.getComputedWidth()},
    {"height",node.getComputedHeight()},
    {"borderLeft",node.getComputedBorder("left")},
    {"paddingTop",node.getComputedPadding("top")},
    {"display",node.getDisplay()==terminal_dom::LayoutDisplay::Flex?
      "flex":"none"}};
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>(std::cin),{});
  const auto cases=value::parse(input);
  Json output=Json::array();
  for(const auto& item:cases){
    auto root=terminal_dom::createLayoutNode();
    auto child=terminal_dom::createLayoutNode();
    terminal_dom::style::applyStyles(*root,item.at("root"));
    terminal_dom::style::applyStyles(*child,item.at("child"));
    root->insertChild(*child,0);
    root->calculateLayout(item.at("width").get<double>(),
      item.at("height").get<double>());
    output.push_back({{"root",show(*root)},{"child",show(*child)}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
