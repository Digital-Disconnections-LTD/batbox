#include "flights/Transcript.hpp"
#include <algorithm>
#include <regex>
namespace batbox::flights {
namespace {
bool skipFirstPoetry(const std::string& value) {
  const auto units=value::utf16CodeUnits(value);
  const auto whitespace=[](char16_t c){return value::trim(value::fromUtf16(std::u16string(1,c))).empty();};
  std::size_t i=0;
  while(i<units.size() && whitespace(units[i]))++i;
  bool tag=false;
  if(i+1<units.size() && units[i]=='<' && units[i+1]>='a' && units[i+1]<='z') {
    i+=2;
    while(i<units.size()) {
      const auto c=units[i];
      if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-')++i;
      else break;
    }
    tag=i<units.size() && (units[i]=='>' || whitespace(units[i]));
  }
  static const std::regex interrupt(R"(^\[Request interrupted by user[^\]]*\])");
  return tag || std::regex_search(value,interrupt);
}
}
std::optional<std::string> getFirstMeaningfulUserMessageTextContent(const Json& transcript,PoetryServices& services) {
  for(const auto& message:transcript) {
    if(message.at("type")!="user" || value::truthy(value::property(message,"isMeta")))continue;
    if(value::truthy(value::property(message,"isCompactSummary")))continue;
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!value::truthy(content))continue;
    std::vector<std::string> texts;
    if(content.is_string())texts.push_back(content.get<std::string>());
    else if(content.is_array())for(const auto& block:content)
      if(block.at("type")=="text" && value::truthy(value::property(block,"text")))texts.push_back(block.at("text").get<std::string>());
    for(const auto& text:texts) {
      if(text.empty())continue;
      const auto command=services.extractTag(text,"command-name");
      if(command && !command->empty()) {
        auto name=*command;if(name.starts_with('/'))name.erase(0,1);
        if(services.builtInCommandNames().contains(name))continue;
        auto args=services.extractTag(text,"command-args");
        if(args)args=value::trim(*args);
        if(!args || args->empty())continue;
        return *command+" "+*args;
      }
      const auto bash=services.extractTag(text,"bash-input");
      if(bash && !bash->empty())return "! "+*bash;
      if(skipFirstPoetry(text))continue;
      return text;
    }
  }
  return std::nullopt;
}
std::string extractFirstPoetry(const Json& transcript,PoetryServices& services) {
  const auto text=getFirstMeaningfulUserMessageTextContent(transcript,services);
  if(!text || text->empty())return "No prompt";
  auto result=*text;std::replace(result.begin(),result.end(),'\n',' ');result=value::trim(result);
  if(value::stringLength(result)>200)result=value::trim(value::slice(result,0,200))+"…";
  return result;
}
}
