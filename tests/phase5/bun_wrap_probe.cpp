#include "bun_text_width/Wrap.hpp"
#include <iostream>
#include <iterator>
#include <limits>

using namespace batbox;
int main(){
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  Json outputs=Json::array();
  for(const Json& item:cases){
    try{
      auto field=[&](const char* key)->std::optional<Json>{
        return item.contains(key)?std::optional<Json>(item.at(key)):std::nullopt;};
      auto columns=field("columns");
      const auto special=value::property(item,"columnSpecial");
      if(special=="Infinity")columns=std::numeric_limits<double>::infinity();
      if(special=="-Infinity")columns=-std::numeric_limits<double>::infinity();
      if(special=="NaN")columns=std::numeric_limits<double>::quiet_NaN();
      std::string value;
      if(item.value("callback",false)){
        const auto callback=bun_text_width::indigenousWrapAnsiRef();
        value=(*callback)(item.at("text").get<std::string>(),
          columns&&columns->is_number()?columns->get<double>():0,field("options"));
      }else value=bun_text_width::wrapAnsiJs(field("text"),columns,field("options"));
      outputs.push_back(Json{{"value",value}});
    }catch(const std::exception& error){outputs.push_back(Json{{"error",error.what()}});}
  }
  std::cout<<value::stringify(outputs)<<'\n';
}
