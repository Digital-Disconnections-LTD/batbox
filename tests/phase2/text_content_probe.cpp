#include "messages/TextContent.hpp"
#include <iostream>
#include <iterator>
int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});
  auto out=batbox::Json::array();
  for(const auto& value:batbox::value::parse(input)){
    auto row=batbox::Json::object();
    if(value.contains("blocks"))row["extract"]=batbox::messages::extractTextContent(
      value.at("blocks"),value.value("separator",std::string()));
    const auto content=batbox::messages::getContentText(value.at("content"));
    row["content"]=content?batbox::Json(*content):batbox::Json();out.push_back(row);
  }
  std::cout<<batbox::value::stringify(out)<<'\n';
}
