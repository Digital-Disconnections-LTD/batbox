#include "gadgets/files/Glob.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  const auto op=t.at("op").get<std::string>();
  if(op=="map")return files::mapGlobResult(t.at("output"),"use-1");
  files::GlobServices s;
  s.paths.getCwd=[&](){return t.at("cwd").get<std::string>();};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.platform=[&](){return t.value("platform",std::string("linux"));};
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
    if(t.contains("rgFiles"))for(const auto& x:t.at("rgFiles"))result.push_back(x.get<std::string>());return result;};
  s.noIgnore=[&](){return t.value("noIgnore",true);};
  s.hidden=[&](){return t.value("hidden",true);};
  double now=1000;s.nowMs=[&](){const double value=now;now+=7;return value;};
  if(op=="base"){
    const auto base=files::extractGlobBaseDirectory(t.at("pattern").get<std::string>(),s);
    return Json{{"baseDir",base.baseDir},{"relativePattern",base.relativePattern}};
  }
  if(op=="validate"){
    const auto v=files::validateGlobInput(t.at("input"),s);
    Json result={{"result",v.result}};if(v.message)result["message"]=*v.message;
    if(v.errorCode)result["errorCode"]=*v.errorCode;return result;
  }
  batbox::gadgets::GadgetCallContext context{};context.data=Json::object();
  if(t.contains("globLimits"))context.data["globLimits"]=t.at("globLimits");
  const auto result=files::callGlob(t.at("input"),context,s);
  return Json{{"data",result.data},{"events",events}};
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
