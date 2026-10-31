#include "bun_text_width/Width.hpp"
#include <iostream>
#include <iterator>

using namespace batbox;
int main(){
  const auto input=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  Json output=Json::array();
  for(const Json& item:input){
    try{
      const std::optional<Json> value=item.contains("text")?
        std::optional<Json>(item.at("text")):std::nullopt;
      const Json options=item.contains("options")?item.at("options"):Json();
      const double width=item.value("callback",false)?
        bun_text_width::indigenousWidthCallback()(
          value?*value:Json(Json::value_t::discarded),options):
        bun_text_width::stringWidth(value,options);
      output.push_back(Json{{"width",width}});
    }catch(const std::exception& error){output.push_back(Json{{"error",error.what()}});}
  }
  std::cout<<value::stringify(output)<<'\n';
}
