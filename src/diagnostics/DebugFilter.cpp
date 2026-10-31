#include "diagnostics/DebugFilter.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>

namespace batbox::diagnostics {
namespace {
std::string lower(std::string value){
  for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
void appendUnique(std::vector<std::string>& values,const std::string& value){
  if(std::find(values.begin(),values.end(),value)==values.end())values.push_back(value);
}
}
std::optional<DebugFilter> parseDebugFilter(const std::optional<std::string>& pattern){
  if(!pattern||value::trim(*pattern).empty())return std::nullopt;
  std::vector<std::string> parts;
  std::stringstream input(*pattern);
  std::string part;
  while(std::getline(input,part,',')){
    part=value::trim(part);
    if(!part.empty())parts.push_back(part);
  }
  if(parts.empty())return std::nullopt;
  const bool hasExclude=std::any_of(parts.begin(),parts.end(),
    [](const std::string& s){return s.starts_with('!');});
  const bool hasInclude=std::any_of(parts.begin(),parts.end(),
    [](const std::string& s){return !s.starts_with('!');});
  if(hasExclude&&hasInclude)return std::nullopt;
  DebugFilter filter;filter.isExclusive=hasExclude;
  for(auto part:parts){
    if(part.starts_with('!'))part.erase(part.begin());
    part=lower(part);
    if(hasExclude)filter.exclude.push_back(part);
    else filter.include.push_back(part);
  }
  return filter;
}
std::vector<std::string> extractDebugCategories(const std::string& message){
  std::vector<std::string> categories;
  std::smatch match;
  static const std::regex mcp(R"re(^MCP server ["']([^"']+)["'])re");
  static const std::regex prefix(R"(^([^:\[]+):)");
  static const std::regex bracket(R"(^\[([^\]]+)])");
  static const std::regex secondary(
    R"(:\s*([^:]+?)(?:\s+(?:type|mode|status|event))?:)");
  if(std::regex_search(message,match,mcp)){
    appendUnique(categories,"mcp");appendUnique(categories,lower(match[1]));
  }else if(std::regex_search(message,match,prefix))
    appendUnique(categories,lower(value::trim(match[1].str())));
  if(std::regex_search(message,match,bracket))
    appendUnique(categories,lower(value::trim(match[1].str())));
  if(lower(message).find("1p event:")!=std::string::npos)
    appendUnique(categories,"1p");
  if(std::regex_search(message,match,secondary)){
    const auto value=lower(value::trim(match[1].str()));
    if(value::stringLength(value)<30&&value.find(' ')==std::string::npos)
      appendUnique(categories,value);
  }
  return categories;
}
bool shouldShowDebugCategories(const std::vector<std::string>& categories,
    const std::optional<DebugFilter>& filter){
  if(!filter)return true;
  if(categories.empty())return false;
  if(filter->isExclusive){
    for(const auto& category:categories)
      if(std::find(filter->exclude.begin(),filter->exclude.end(),category)!=
          filter->exclude.end())return false;
    return true;
  }
  for(const auto& category:categories)
    if(std::find(filter->include.begin(),filter->include.end(),category)!=
        filter->include.end())return true;
  return false;
}
bool shouldShowDebugMessage(const std::string& message,
    const std::optional<DebugFilter>& filter){
  if(!filter)return true;
  return shouldShowDebugCategories(extractDebugCategories(message),filter);
}
}
