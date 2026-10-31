#include "skills/Skill.hpp"
#include "runtime/JsonValue.hpp"
#include <regex>
#include <stdexcept>

namespace batbox::skills {
namespace {
bool whitespace(char16_t unit){
  return (unit>=0x09&&unit<=0x0D)||unit==0x20||unit==0xA0||
    unit==0x1680||(unit>=0x2000&&unit<=0x200A)||unit==0x2028||
    unit==0x2029||unit==0x202F||unit==0x205F||unit==0x3000||unit==0xFEFF;
}
std::vector<std::string> fallbackSplit(const std::string& args){
  const auto units=value::utf16CodeUnits(args);
  std::vector<std::string> tokens;
  for(std::size_t i=0;i<units.size();){
    while(i<units.size()&&whitespace(units[i]))++i;
    if(i==units.size())break;
    const auto start=i;
    while(i<units.size()&&!whitespace(units[i]))++i;
    tokens.push_back(value::fromUtf16(std::u16string_view(units).substr(start,i-start)));
  }
  return tokens;
}
std::string argAt(const std::vector<std::string>& arguments,const std::string& index){
  try{
    const auto parsed=std::stoull(index);
    return parsed<arguments.size()?arguments[parsed]:std::string{};
  }catch(const std::exception&){return {};}
}
template<class Replacement>
std::string replaceMatches(const std::string& content,const std::regex& pattern,
  Replacement&& replacement){
  std::string output;std::size_t end=0;
  for(std::sregex_iterator it(content.begin(),content.end(),pattern),last;
      it!=last;++it){
    const auto& match=*it;
    const auto start=static_cast<std::size_t>(match.position());
    output.append(content,end,start-end);
    output+=replacement(match);
    end=start+static_cast<std::size_t>(match.length());
  }
  output.append(content,end,std::string::npos);
  return output;
}
}
std::vector<std::string> parseArguments(const std::string& args,
  const SkillServices& services){
  if(args.empty()||value::trim(args).empty())return {};
  if(!services.tryParseShellCommand)
    throw std::invalid_argument("Missing skill binding: tryParseShellCommand");
  const auto parsed=services.tryParseShellCommand(args);
  return parsed?*parsed:fallbackSplit(args);
}
std::string substituteArguments(const std::string& rawContent,
  const std::optional<std::string>& args,bool appendIfNoPlaceholder,
  const std::vector<std::string>& argumentNames,const SkillServices& services){
  if(!args)return rawContent;
  const auto parsed=parseArguments(*args,services);
  std::string content=rawContent;
  for(std::size_t i=0;i<argumentNames.size();++i){
    const auto& name=argumentNames[i];
    if(name.empty())continue;
    const std::regex pattern("\\$"+name+"(?![\\[\\w])");
    content=replaceMatches(content,pattern,[&](const auto&){
      return i<parsed.size()?parsed[i]:std::string{};
    });
  }
  static const std::regex indexed(R"(\$ARGUMENTS\[(\d+)\])");
  content=replaceMatches(content,indexed,[&](const auto& match){
    return argAt(parsed,match[1].str());
  });
  static const std::regex shorthand(R"(\$(\d+)(?!\w))");
  content=replaceMatches(content,shorthand,[&](const auto& match){
    return argAt(parsed,match[1].str());
  });
  for(std::size_t position=0;(position=content.find("$ARGUMENTS",position))!=
      std::string::npos;position+=args->size())
    content.replace(position,10,*args);
  if(content==rawContent&&appendIfNoPlaceholder&&!args->empty())
    content+="\n\nARGUMENTS: "+*args;
  return content;
}
}
