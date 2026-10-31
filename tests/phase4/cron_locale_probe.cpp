#include "cron/locale/NotificationDate.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <optional>
#include <string>

namespace {
std::optional<std::string> optional(const batbox::Json& value){
  return value.is_string()?std::optional(value.get<std::string>()):std::nullopt;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto item=batbox::value::parse(line);
    const auto value=batbox::cron::locale::formatLocalDateTime(
      item.at("epochMs").get<double>(),optional(batbox::value::property(item,"locale")),
      optional(batbox::value::property(item,"timeZone")));
    std::cout<<batbox::value::stringify(batbox::Json{{"value",value}})<<'\n';
  }
}
