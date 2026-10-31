#include "application_lsp/Format.hpp"
#include <iostream>
#include <iterator>

int main(){
  using batbox::Json;
  std::string input((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
  auto cases=Json::parse(input);
  Json output=Json::array();
  for(const auto& item:cases){
    auto result=batbox::application_lsp::formatResult(
      item.at("operation").get<std::string>(),item.at("result"),
      item.at("cwd").get<std::string>());
    output.push_back(result.text);
  }
  std::cout<<output.dump()<<'\n';
}
