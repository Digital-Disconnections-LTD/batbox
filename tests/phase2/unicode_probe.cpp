#include "runtime/JsonValue.hpp"
#include <iostream>
#include <cmath>
using namespace batbox;
Json run(const Json& input) {
  const auto op=input.at("op").get<std::string>();
  if(op=="parse")return value::parse(input.at("text").get<std::string>());
  const auto text=input.at("text").get<std::string>();
  if(op=="parseInt") {
    const auto value=value::parseInt10(text);
    Json encoded=std::isfinite(value) ? Json(value) : Json(std::isnan(value) ? "NaN" : value<0 ? "-Infinity" : "Infinity");
    return {{"value",encoded},{"negativeZero",value==0 && std::signbit(value)}};
  }
  if(op=="length")return value::stringLength(text);
  if(op=="stringify")return value::stringify(input.at("value"));
  const auto start=input.value("start",0.0);
  const auto end=input.value("end",std::numeric_limits<double>::infinity());
  if(op=="substring")return value::substring(text,start,end);
  if(op=="slice")return value::slice(text,start,end);
  throw std::runtime_error("Unknown Unicode probe operation");
}
int main(){
  for(std::string line;std::getline(std::cin,line);){
    try{std::cout<<value::stringify(Json{{"result",run(value::parse(line))}})<<'\n';}
    catch(const Json::parse_error&){std::cout<<"{\"error\":\"SyntaxError\"}\n";}
    catch(const std::exception& e){std::cout<<value::stringify(Json{{"error",e.what()}})<<'\n';}
  }
}
