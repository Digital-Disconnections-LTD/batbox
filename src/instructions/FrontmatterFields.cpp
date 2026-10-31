#include "instructions/Frontmatter.hpp"
#include <algorithm>
#include <cctype>
#include <regex>

namespace batbox::instructions {
namespace {
std::string jsString(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_null())return "null";
  if(value.is_array()){
    std::string out;bool first=true;
    for(const auto& item:value){if(!first)out+=',';first=false;
      if(!item.is_null())out+=jsString(item);}
    return out;
  }
  return "[object Object]";
}
}
std::optional<std::string> coerceDescriptionToString(const Json& value){
  if(value.is_null())return std::nullopt;
  if(value.is_string()){
    const auto trimmed=value::trim(value.get<std::string>());
    return trimmed.empty()?std::nullopt:std::optional<std::string>(trimmed);
  }
  if(value.is_number()||value.is_boolean())return jsString(value);
  return std::nullopt;
}
bool parseBooleanFrontmatter(const Json& value){return value==true||value=="true";}
std::optional<std::string> parseShellFrontmatter(const Json& value,
  const std::string& source,const std::function<void(const std::string&)>& logDebug){
  if(value.is_null())return std::nullopt;
  auto normalized=value::trim(jsString(value));
  std::transform(normalized.begin(),normalized.end(),normalized.begin(),
    [](unsigned char c){return static_cast<char>(std::tolower(c));});
  if(normalized.empty())return std::nullopt;
  if(normalized=="bash"||normalized=="powershell")return normalized;
  if(logDebug)logDebug("Frontmatter 'shell: "+jsString(value)+"' in "+source+
    " is not recognized. Valid values: bash, powershell. Falling back to bash.");
  return std::nullopt;
}
std::string extractDescriptionFromMarkdown(const std::string& content,
  const std::string& defaultDescription){
  static const std::regex header(R"(^#+\s+(.+)$)");
  for(std::size_t start=0;;){
    const auto end=content.find('\n',start);
    const auto trimmed=value::trim(content.substr(start,
      end==std::string::npos?end:end-start));
    if(!trimmed.empty()){
      std::smatch match;
      const auto text=std::regex_match(trimmed,match,header)?match[1].str():trimmed;
      return value::stringLength(text)>100?value::substring(text,0,97)+"...":text;
    }
    if(end==std::string::npos)break;
    start=end+1;
  }
  return defaultDescription;
}
} // namespace batbox::instructions
