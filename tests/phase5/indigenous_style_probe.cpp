#include "terminal_dom/style/Style.hpp"
#include "indigenous_style_trace.hpp"
#include <iostream>
#include <iterator>

int main(){
  using namespace batbox;
  const std::string input(std::istreambuf_iterator<char>(std::cin),{});
  const auto cases=value::parse(input);
  Json output=Json::array();
  for(const auto& item:cases){
    terminal_dom::style_test::TraceLayoutNode node;
    const auto& value=item.at("style");
    const auto* resolved=item.contains("resolved")?&item.at("resolved"):nullptr;
    try{
      terminal_dom::style::applyStyles(node,value,resolved);
      output.push_back(std::move(node.trace));
    }catch(const terminal_dom::style::TypeError& error){
      output.push_back({{"error","TypeError"},{"message",error.what()},
        {"trace",node.trace}});
    }
  }
  std::cout<<value::stringify(output)<<'\n';
}
