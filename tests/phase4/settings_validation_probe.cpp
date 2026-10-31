#include "settings/Validation.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
namespace {
Json run(const Json& input){
  const auto docs=std::string("https://docs.batbox.test/en");
  const auto op=value::property(input,"op").get<std::string>();
  Json result;
  if(op=="tip"){
    const auto tip=settings::getValidationTip(value::property(input,"context"),docs);
    if(!tip.suggestion&&!tip.docLink)result=nullptr;
    else{
      result=Json::object();
      if(tip.suggestion)result["suggestion"]=*tip.suggestion;
      if(tip.docLink)result["docLink"]=*tip.docLink;
    }
  }else if(op=="format"){
    const auto& path=value::property(input,"path");
    result=settings::formatZodError(value::property(input,"issues"),
      path.is_string()?path.get<std::string>():"settings.json",docs);
  }else throw std::invalid_argument("Unknown operation");
  return {{"result",result}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
