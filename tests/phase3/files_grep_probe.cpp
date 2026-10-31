#include "gadgets/files/Grep.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  const auto op=t.at("op").get<std::string>();
  if(op=="head"){
    std::vector<std::string> values;for(const auto& x:t.at("items"))values.push_back(x.get<std::string>());
    const auto limit=t.contains("limit")?std::optional<double>(t.at("limit").get<double>()):std::nullopt;
    const auto result=files::applyHeadLimit(values,limit,t.value("offset",0.0));
    Json output={{"items",result.items}};if(result.appliedLimit)output["appliedLimit"]=*result.appliedLimit;
    return output;
  }
  if(op=="limitInfo")return files::formatLimitInfo(
    t.contains("limit")?std::optional<double>(t.at("limit").get<double>()):std::nullopt,
    t.contains("offset")?std::optional<double>(t.at("offset").get<double>()):std::nullopt);
  if(op=="map")return files::mapGrepResult(t.at("output"),"use-1");
  files::GrepServices s;
  s.paths.getCwd=[&](){return t.at("cwd").get<std::string>();};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.suggestPathUnderCwd=[&](const std::string&)->std::optional<std::string>{
    if(t.contains("suggestion"))return t.at("suggestion").get<std::string>();return std::nullopt;};
  s.getFileReadIgnorePatterns=[&](const Json&){std::vector<std::string> result;
    if(t.contains("ignorePatterns"))for(const auto& x:t.at("ignorePatterns"))result.push_back(x.get<std::string>());return result;};
  s.normalizePatternsToPath=[](const std::vector<std::string>& values,const std::string&){return values;};
  s.getGlobExclusionsForPluginStash=[&](const std::string&){std::vector<std::string> result;
    if(t.contains("exclusions"))for(const auto& x:t.at("exclusions"))result.push_back(x.get<std::string>());return result;};
  std::vector<Json> events;
  s.ripGrep=[&](const std::vector<std::string>& args,const std::string& dir,std::stop_token){
    events.push_back(Json{{"args",args},{"dir",dir}});
    std::vector<std::string> result;
    if(t.contains("rgResults"))for(const auto& x:t.at("rgResults"))result.push_back(x.get<std::string>());return result;};
  s.localeCompare=[](const std::string& a,const std::string& b){return a.compare(b);};
  s.testEnvironment=true;
  if(op=="validate"){
    const auto v=files::validateGrepInput(t.at("input"),s);
    Json result={{"result",v.result}};if(v.message)result["message"]=*v.message;
    if(v.errorCode)result["errorCode"]=*v.errorCode;return result;
  }
  batbox::gadgets::GadgetCallContext context{};context.data=Json::object();
  const auto result=files::callGrep(t.at("input"),context,s);
  return Json{{"data",result.data},{"events",events}};
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
