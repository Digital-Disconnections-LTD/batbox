#include "auth/OAuthScopes.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    std::optional<std::vector<std::string>> scopes;
    if(test.contains("scopes")&&!test["scopes"].is_null())
      scopes=test["scopes"].get<std::vector<std::string>>();
    std::optional<std::string> scopeString;
    if(test.contains("scopeString")&&!test["scopeString"].is_null())
      scopeString=test["scopeString"].get<std::string>();
    std::optional<double> expiresAt;
    if(test.contains("expiresAt")&&!test["expiresAt"].is_null())
      expiresAt=test["expiresAt"].get<double>();
    Json trace=Json::array();
    const auto expired=auth::isOAuthTokenExpired(expiresAt,[&](){
      trace.push_back("now");return test.value("now",1000.0);});
    std::cout<<value::stringify(Json{
      {"use",auth::shouldUseBatboxAuth(scopes,"user:inference")},
      {"parsed",auth::parseScopes(scopeString)},
      {"expired",expired},{"trace",trace}})<<'\n';
  }
}
