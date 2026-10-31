#include "formatting/Format.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  auto input=Json::parse(line);auto value=input.at("size");double number;
  if(value.is_string()){
    auto text=value.get<std::string>();
    number=text=="NaN"?std::numeric_limits<double>::quiet_NaN():
      text=="Infinity"?std::numeric_limits<double>::infinity():-std::numeric_limits<double>::infinity();
  }else number=value.get<double>();
  std::cout<<Json{{"result",batbox::formatting::formatFileSize(number)}}.dump()<<'\n';
 }catch(const std::exception& e){std::cout<<Json{{"fatal",e.what()}}.dump()<<'\n';}}
