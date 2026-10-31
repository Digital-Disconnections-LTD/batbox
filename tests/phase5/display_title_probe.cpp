#include "display/Tags.hpp"
#include "flights/DisplayTitle.hpp"
#include <iostream>
#include <iterator>
int main(){
  using namespace batbox;
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});Json out=Json::array();
  for(const auto& item:value::parse(input)){
    const auto text=item.at("text").get<std::string>();
    const auto fallback=item.contains("defaultTitle")?
      std::optional<std::string>(item.at("defaultTitle").get<std::string>()):std::nullopt;
    out.push_back(Json{{"strip",display::stripDisplayTags(text)},
      {"empty",display::stripDisplayTagsAllowEmpty(text)},
      {"ide",display::stripIdeContextTags(text)},
      {"title",flights::getLogDisplayTitle(item.at("log"),fallback)}});
  }
  std::cout<<value::stringify(out)<<'\n';
}
