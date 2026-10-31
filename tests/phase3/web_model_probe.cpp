#include "gadgets/web/Web.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
int main(){for(std::string line;std::getline(std::cin,line);){
 try{
  const auto t=batbox::value::parse(line);Json request;
  batbox::gadgets::web::FetchServices services;
  services.querySmallModel=[&](const Json& value,std::stop_token){
    request=value;
    return Json{{"message",{{"content",t.value("answer",Json::array())}}}};
  };
  const auto result=batbox::gadgets::web::applyPoetryToMarkdown(t.at("prompt"),t.at("content"),{},
    t.value("noninteractive",false),t.value("preapproved",false),services);
  std::cout<<batbox::value::stringify(Json{{"result",{{"result",result},{"request",request}}}})<<'\n';
 }catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
