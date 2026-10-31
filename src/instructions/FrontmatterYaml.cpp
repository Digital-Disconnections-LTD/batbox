#include "instructions/Frontmatter.hpp"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>

namespace batbox::instructions {
namespace {
bool plainNumber(const std::string& text){
  static const std::regex number(R"(^[+-]?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?$|^[+-]?\.[0-9]+(?:[eE][+-]?[0-9]+)?$)");
  return std::regex_match(text,number);
}
Json scalar(const YAML::Node& node){
  const auto value=node.Scalar();
  if(node.Tag()=="!")return value;
  std::string lower=value;
  for(char& c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if(lower=="null"||lower=="~")return nullptr;
  if(lower=="true")return true;
  if(lower=="false")return false;
  if(lower==".inf"||lower=="+.inf")return std::numeric_limits<double>::infinity();
  if(lower=="-.inf")return -std::numeric_limits<double>::infinity();
  if(lower==".nan")return std::numeric_limits<double>::quiet_NaN();
  try{
    std::size_t consumed=0;
    if(lower.starts_with("0x")||lower.starts_with("0o")||lower.starts_with("0b")){
      const int base=lower[1]=='x'?16:lower[1]=='o'?8:2;
      const auto number=std::stoull(lower.substr(2),&consumed,base);
      if(consumed==lower.size()-2)return static_cast<double>(number);
    }else if(plainNumber(value)){
      const double number=std::stod(value,&consumed);
      if(consumed==value.size())return number;
    }
  }catch(const std::exception&){}
  return value;
}
Json convert(const YAML::Node& node,int depth=0){
  if(depth>100)throw std::runtime_error("YAML nesting too deep");
  if(!node||node.IsNull())return nullptr;
  if(node.IsScalar())return scalar(node);
  if(node.IsSequence()){
    Json result=Json::array();
    for(const auto& item:node)result.push_back(convert(item,depth+1));
    return result;
  }
  if(node.IsMap()){
    Json result=Json::object();std::set<std::string> seen;
    for(const auto& item:node){
      if(!item.first.IsScalar())throw std::runtime_error("Non-scalar YAML key");
      const auto key=item.first.Scalar();
      if(!seen.insert(key).second)throw std::runtime_error("Duplicate YAML key");
      result[key]=convert(item.second,depth+1);
    }
    return result;
  }
  return nullptr;
}
std::string quoteProblematicValues(const std::string& text){
  static const std::regex keyValue(R"(^([a-zA-Z_-]+):\s+(.+)$)");
  std::string result;bool first=true;
  for(std::size_t start=0;;){
    const auto end=text.find('\n',start);
    auto line=text.substr(start,end==std::string::npos?end:end-start);
    std::smatch match;
    if(std::regex_match(line,match,keyValue)&&match.size()==3){
      const auto value=match[2].str();
      const bool quoted=value.size()>=2&&((value.front()=='"'&&value.back()=='"')||
        (value.front()=='\''&&value.back()=='\''));
      const bool special=value.find_first_of("{}[]*&#!|>%@`")!=std::string::npos||
        value.find(": ")!=std::string::npos;
      if(!quoted&&special){
        std::string escaped;
        for(char c:value){if(c=='\\'||c=='"')escaped+='\\';escaped+=c;}
        line=match[1].str()+": \""+escaped+"\"";
      }
    }
    if(!first)result+='\n';
    result+=line;
    first=false;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return result;
}
Json parseYamlObject(const std::string& text){
  const auto node=YAML::Load(text);
  const auto result=convert(node);
  return result.is_object()?result:Json::object();
}
}
ParsedMarkdown parseFrontmatter(const std::string& markdown,
  const std::optional<std::string>& sourcePath,
  const std::function<void(const std::string&)>& logDebug){
  static const std::regex frontmatter(R"(^---\s*\n([\s\S]*?)---\s*\n?)");
  std::smatch match;
  if(!std::regex_search(markdown,match,frontmatter))return {Json::object(),markdown};
  const auto yaml=match[1].str();
  const auto content=markdown.substr(static_cast<std::size_t>(match.length()));
  try{return {parseYamlObject(yaml),content};}
  catch(const std::exception&){
    try{return {parseYamlObject(quoteProblematicValues(yaml)),content};}
    catch(const std::exception& error){
      if(logDebug)logDebug("Failed to parse YAML frontmatter"+
        (sourcePath?" in "+*sourcePath:"")+": "+error.what());
      return {Json::object(),content};
    }
  }
}
} // namespace batbox::instructions
