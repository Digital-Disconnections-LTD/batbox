#include "gadgets/web/Web.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
int main(){for(std::string line;std::getline(std::cin,line);){
 try{
  const auto t=batbox::value::parse(line);
  const std::vector<std::string> approved={"docs.python.org","vercel.com/docs","github.com/some-project"};
  Json result;
  if(t.at("op")=="host")result=batbox::gadgets::web::isPreapprovedHost(t.at("host"),t.at("path"),approved);
  else if(t.at("op")=="content")result=batbox::gadgets::web::webFetchPermissionRuleContent(t.at("input"));
  else{
   batbox::gadgets::web::FetchServices s;s.preapprovedHosts=approved;
   s.permissionRule=[&](const Json&,const std::string& mode,const std::string& content)->std::optional<Json>{
    const auto rules=batbox::value::property(batbox::value::property(t,"rules"),mode);
    if(rules.is_object()&&rules.contains(content))return rules.at(content);
    return std::nullopt;};
   result=batbox::gadgets::web::checkFetchPermissions(t.at("input"),Json::object(),s);
  }
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
 }catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
