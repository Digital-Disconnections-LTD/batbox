#include "runtime/Url.hpp"
#include <iostream>
using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto input=value::parse(line);const auto value=input.at("value").get<std::string>();
    const auto base=input.contains("base")?std::optional<std::string>(input.at("base").get<std::string>()):std::nullopt;
    const auto parsed=runtime::parseUrl(value,base);
    Json result={{"absoluteValid",runtime::canParseUrl(value)},{"valid",bool(parsed)}};
    const auto absolute=runtime::canonicalUrl(value);
    if(absolute)result["absoluteHref"]=*absolute;
    if(parsed)result["url"]=*parsed;
    std::cout<<value::stringify(result)<<'\n';
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
