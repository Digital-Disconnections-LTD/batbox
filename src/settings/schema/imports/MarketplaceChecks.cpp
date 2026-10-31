#include "settings/schema/imports/MarketplaceInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>

namespace batbox::settings::schema::imports::detail {
namespace {
std::string lower(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char ch){
    return static_cast<char>(std::tolower(ch));});return value;
}
bool reserved(const std::string& name){
  const std::string names[]={"batbox-code-marketplace","batbox-code-plugins",
    "batbox-plugins-official","batbox-marketplace","batbox-plugins",
    "agent-skills","life-sciences","knowledge-work-plugins"};
  const auto normalized=lower(name);
  return std::find(std::begin(names),std::end(names),normalized)!=
    std::end(names);
}
bool blockedOfficialName(const std::string& name){
  if(reserved(name))return false;
  for(unsigned char ch:name)if(ch<0x20||ch>0x7e)return true;
  static const std::regex pattern(
    R"((?:official[^a-z0-9]*batbox|batbox[^a-z0-9]*official|^batbox[^a-z0-9]*(marketplace|plugins|official)))",
    std::regex::icase);
  return std::regex_search(name,pattern);
}
bool customPass(const std::string& name,const Json& input){
  const auto text=input.is_string()?input.get<std::string>():std::string();
  if(name.find("package#0")!=std::string::npos||
      name.find("package|0#0")!=std::string::npos)
    return text.find("..") ==std::string::npos&&
      text.find("//")==std::string::npos;
  if(name.find("package#1")!=std::string::npos||
      name.find("package|0#1")!=std::string::npos){
    static const std::regex scoped(
      R"(^@[a-z0-9][a-z0-9-._]*/[a-z0-9][a-z0-9-._]*$)");
    static const std::regex regular(R"(^[a-z0-9][a-z0-9-._]*$)");
    return std::regex_match(text,scoped)||std::regex_match(text,regular);
  }
  if(name.find("plugins[].name#1")!=std::string::npos)
    return text.find(' ')==std::string::npos;
  if(name.find("marketplace|8.name#1")!=std::string::npos)
    return text.find(' ')==std::string::npos;
  if(name.find("marketplace|8.name#2")!=std::string::npos)
    return text.find('/')==std::string::npos&&
      text.find('\\')==std::string::npos&&
      text.find("..")==std::string::npos&&text!=".";
  if(name.find("marketplace|8.name#3")!=std::string::npos)
    return !blockedOfficialName(text);
  if(name.find("marketplace|8.name#4")!=std::string::npos)
    return lower(text)!="inline";
  if(name.find("marketplace|8.name#5")!=std::string::npos)
    return lower(text)!="builtin";
  if(name.find("marketplace|8.name#6")!=std::string::npos)
    return !reserved(text);
  if(name.find("plugins[]#0")!=std::string::npos)
    return !input.is_object()||!input.contains("source")||
      !input["source"].is_string();
  throw std::runtime_error("Unbound marketplace custom check: "+name);
}
void lengthIssue(const Json& path,Json& issues,
  std::size_t actual,int expected,const std::string& error,bool exact){
  if(actual>=static_cast<std::size_t>(expected))return;
  Json issue={{"origin","string"},{"code","too_small"},
    {"minimum",expected},{"inclusive",true}};
  if(exact)issue["exact"]=true;
  issue["path"]=path;
  issue["message"]=error.empty()?
    "Too small: expected string to have >="+
      std::to_string(expected)+" characters":error;
  issues.push_back(issue);
}
}
void applyMarketplaceChecks(const Json& node,const Json& input,
  const Json& path,Json& output,Json& issues,
  const UrlCanonicalizer& canonicalUrl){
  if(!node.contains("checks"))return;
  Json current=input;
  for(const auto& check:node["checks"]){
    const auto kind=check.at("kind").get<std::string>();
    const auto error=check.value("error",std::string());
    if(kind=="custom"){
      if(!customPass(check.at("name").get<std::string>(),current))
        issues.push_back({{"code","custom"},{"path",path},
          {"message",error}});
    }else if(kind=="min_length"){
      lengthIssue(path,issues,value::stringLength(current.get<std::string>()),
        check.at("minimum").get<int>(),error,false);
    }else if(kind=="length_equals"){
      const auto actual=value::stringLength(current.get<std::string>());
      const int expected=check.at("length").get<int>();
      lengthIssue(path,issues,actual,expected,error,true);
      if(actual>static_cast<std::size_t>(expected))
        issues.push_back({{"origin","string"},{"code","too_big"},
          {"maximum",expected},{"inclusive",true},{"exact",true},
          {"path",path},{"message","Too big: expected string to have <="+
            std::to_string(expected)+" characters"}});
    }else if(kind=="string_format"){
      const auto format=check.at("format").get<std::string>();
      const auto text=current.get<std::string>();
      if(format=="url"){
        if(!canonicalUrl)throw std::runtime_error("Missing marketplace URL parser");
        const auto href=canonicalUrl(text);
        if(!href)issues.push_back({{"code","invalid_format"},
          {"format","url"},{"path",path},{"message","Invalid URL"}});
        else current=!text.ends_with('/')&&href->ends_with('/')
          ?href->substr(0,href->size()-1):*href;
      }else if(format=="starts_with"){
        if(text.rfind("./",0)!=0)
          issues.push_back({{"origin","string"},{"code","invalid_format"},
            {"format","starts_with"},{"prefix","./"},{"path",path},
            {"message","Invalid string: must start with \"./\""}});
      }else if(format=="regex"){
        static const std::regex sha(R"(^[a-f0-9]{40}$)");
        if(!std::regex_match(text,sha))
          issues.push_back({{"origin","string"},{"code","invalid_format"},
            {"format","regex"},{"pattern",check.at("pattern")},
            {"path",path},{"message",error}});
      }else throw std::runtime_error("Unbound marketplace format: "+format);
    }else throw std::runtime_error("Unbound marketplace check: "+kind);
  }
  if(input.is_string())output=current;
}
}
