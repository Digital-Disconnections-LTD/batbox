#include "auth/oauth_client/ProfileScope.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    const auto value=auth::oauth_client::hasProfileScope(
      [&]()->std::optional<Json>{trace.push_back("tokens");
        return test.contains("tokens")&&
          !test["tokens"].is_null()?std::optional(test["tokens"]):
          std::nullopt;},"user:profile");
    std::cout<<value::stringify(Json{{"value",value},{"trace",trace}})<<'\n';
  }
}
