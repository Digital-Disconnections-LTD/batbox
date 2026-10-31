#include "instructions/Frontmatter.hpp"
#include <regex>

namespace batbox::instructions {
namespace {
std::vector<std::string> expandBraces(const std::string& pattern){
  static const std::regex braces(R"(^([^{]*)\{([^}]+)\}(.*)$)");
  std::smatch match;
  if(!std::regex_match(pattern,match,braces))return {pattern};
  const auto prefix=match[1].str(),alternatives=match[2].str(),suffix=match[3].str();
  std::vector<std::string> result;
  for(std::size_t start=0;;){
    const auto end=alternatives.find(',',start);
    const auto expanded=expandBraces(prefix+value::trim(alternatives.substr(start,
      end==std::string::npos?end:end-start))+suffix);
    result.insert(result.end(),expanded.begin(),expanded.end());
    if(end==std::string::npos)break;
    start=end+1;
  }
  return result;
}
}
std::vector<std::string> splitPathInFrontmatter(const Json& input){
  if(input.is_array()){
    std::vector<std::string> result;
    for(const auto& item:input){
      const auto part=splitPathInFrontmatter(item);
      result.insert(result.end(),part.begin(),part.end());
    }
    return result;
  }
  if(!input.is_string())return {};
  const auto value=input.get<std::string>();
  std::vector<std::string> parts;
  std::string current;int braceDepth=0;
  for(const char c:value){
    if(c=='{'){++braceDepth;current+=c;}
    else if(c=='}'){--braceDepth;current+=c;}
    else if(c==','&&braceDepth==0){
      const auto trimmed=value::trim(current);
      if(!trimmed.empty())parts.push_back(trimmed);
      current.clear();
    }else current+=c;
  }
  const auto trimmed=value::trim(current);
  if(!trimmed.empty())parts.push_back(trimmed);
  std::vector<std::string> result;
  for(const auto& part:parts){
    const auto expanded=expandBraces(part);
    result.insert(result.end(),expanded.begin(),expanded.end());
  }
  return result;
}
} // namespace batbox::instructions
