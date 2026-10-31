#include "diagnostics/DebugFilter.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  auto input=batbox::Json::parse(std::cin);
  batbox::Json out=batbox::Json::array();
  for(const auto& item:input){
    const auto pattern=item.contains("pattern")&&item["pattern"].is_string()?
      std::optional(item["pattern"].get<std::string>()):std::nullopt;
    const auto filter=batbox::diagnostics::parseDebugFilter(pattern);
    const auto message=item["message"].get<std::string>();
    batbox::Json encoded=nullptr;
    if(filter)encoded={{"include",filter->include},{"exclude",filter->exclude},
      {"isExclusive",filter->isExclusive}};
    out.push_back({{"filter",encoded},
      {"categories",batbox::diagnostics::extractDebugCategories(message)},
      {"shown",batbox::diagnostics::shouldShowDebugMessage(message,filter)}});
  }
  std::cout<<out.dump()<<'\n';
}
