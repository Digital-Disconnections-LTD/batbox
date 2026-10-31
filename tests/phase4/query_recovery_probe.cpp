#include "query/Recovery.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line)){
 try{Json item=batbox::value::parse(line);Json missing=Json::array();
  batbox::query::yieldMissingGadgetResultBlocks(item.at("messages"),item.at("errorMessage").get<std::string>(),
    [](const Json& args){return Json{{"factory","user"},{"args",args}};},
    [&](const Json& value){missing.push_back(value);});
  std::cout<<batbox::value::stringify(Json{{"missing",missing},
    {"withheld",batbox::query::isWithheldMaxOutputTokens(batbox::value::property(item,"check"))}})<<'\n';
 }catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
