#include "history/References.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  Json input;std::cin>>input;
  const auto operation=input.at("operation").get<std::string>();
  Json result;
  if(operation=="lines")result=history::getPastedTextRefNumLines(
    input.at("text").get<std::string>());
  else if(operation=="text")result=history::formatPastedTextRef(
    input.at("id").get<double>(),input.at("numLines").get<double>());
  else if(operation=="image")result=history::formatImageRef(
    input.at("id").get<double>());
  else if(operation=="parse"){
    result=Json::array();
    for(const auto& match:history::parseReferences(input.at("text").get<std::string>()))
      result.push_back(Json::object({{"id",match.id},{"match",match.match},
        {"index",match.index}}));
  }else if(operation=="expand")result=history::expandPastedTextRefs(
    input.at("text").get<std::string>(),input.at("contents"));
  std::cout<<value::stringify(Json::object({{"result",result}}))<<'\n';
}
