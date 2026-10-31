#include "application_completion/Commands.hpp"
#include "commands/Roster.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>

namespace batbox::application_completion {
namespace {
std::string lower(std::string value){
  for(auto& ch:value)ch=static_cast<char>(std::tolower(
    static_cast<unsigned char>(ch)));
  return value;
}
std::string stringOr(const Json& value){
  return value.is_string()?value.get<std::string>():std::string{};
}
bool begins(const std::string& value,const std::string& prefix){
  return value.starts_with(prefix);
}
int score(const Json& command,const std::string& query){
  const auto name=lower(commands::getCommandName(command));
  if(name==query)return 0;
  const auto& aliases=value::property(command,"aliases");
  if(aliases.is_array())for(const auto& alias:aliases)
    if(lower(stringOr(alias))==query)return 1;
  if(begins(name,query))return 2;
  if(aliases.is_array())for(const auto& alias:aliases)
    if(begins(lower(stringOr(alias)),query))return 3;
  // Substring matches rank names before aliases and descriptions;
  // this completion scorer does not calculate fuzzy edit distances.
  if(name.find(query)!=std::string::npos)return 4;
  if(aliases.is_array())for(const auto& alias:aliases)
    if(lower(stringOr(alias)).find(query)!=std::string::npos)return 5;
  const auto description=lower(stringOr(value::property(command,"description")));
  if(description.find(query)!=std::string::npos)return 6;
  return -1;
}
int category(const Json& command){
  const auto& type=value::property(command,"type");
  const auto& source=value::property(command,"source");
  if(type=="local"||type=="local-jsx")return 0;
  if(type=="prompt"&&(source=="userSettings"||source=="localSettings"))
    return 1;
  if(type=="prompt"&&source=="projectSettings")return 2;
  if(type=="prompt"&&source=="policySettings")return 3;
  return 4;
}
std::string matchingAlias(const Json& command,const std::string& query){
  const auto& aliases=value::property(command,"aliases");
  if(!aliases.is_array()||query.empty())return {};
  for(const auto& alias:aliases){
    const auto text=stringOr(alias);
    if(begins(lower(text),query))return text;
  }
  return {};
}
}
std::optional<CommandSpan> findCommandSpan(
    const std::string& text,std::size_t caretUtf16){
  const auto before=value::slice(text,0,caretUtf16);
  if(text.starts_with('/')){
    const auto split=text.find_first_of(" \t\r\n");
    const auto tokenEnd=split==std::string::npos?text.size():split;
    if(before.find_first_of(" \t\r\n")==std::string::npos&&
       before.size()<=tokenEnd)
      return CommandSpan{before.substr(1),0,
        value::stringLength(text.substr(0,tokenEnd)),true};
    return {};
  }
  static const std::regex mid(R"((^|\s)/([a-zA-Z0-9_:-]*)$)");
  std::smatch match;
  if(!std::regex_search(before,match,mid))return {};
  const auto slash=static_cast<std::size_t>(match.position())+
    match[1].str().size();
  std::size_t end=slash+1;
  while(end<text.size()){
    const auto c=static_cast<unsigned char>(text[end]);
    if(!std::isalnum(c)&&c!='_'&&c!=':'&&c!='-')break;
    ++end;
  }
  return CommandSpan{match[2].str(),
    value::stringLength(text.substr(0,slash)),
    value::stringLength(text.substr(0,end)),false};
}
std::vector<Suggestion> commandSuggestions(
    const CommandSpan& span,const Json& commands){
  if(!commands.is_array())throw std::invalid_argument(
    "Completion commands must be an array");
  struct Candidate{Json command;int rank=0;std::size_t order=0;};
  std::vector<Candidate> found;
  const auto query=lower(span.query);
  for(std::size_t index=0;index<commands.size();++index){
    const auto& command=commands[index];
    const auto name=commands::getCommandName(command);
    if(name.empty())continue;
    const bool hidden=value::truthy(value::property(command,"isHidden"));
    const auto rank=query.empty()?category(command):score(command,query);
    if(rank<0)continue;
    if(hidden){
      if(rank!=0||query.empty())continue;
      const bool visibleOverride=std::any_of(commands.begin(),commands.end(),
        [&](const Json& other){
          return !value::truthy(value::property(other,"isHidden"))&&
            lower(commands::getCommandName(other))==query;});
      if(visibleOverride)continue;
    }
    found.push_back({command,rank,index});
  }
  std::stable_sort(found.begin(),found.end(),[&](const Candidate& left,
      const Candidate& right){
    if(left.rank!=right.rank)return left.rank<right.rank;
    const auto lhs=commands::getCommandName(left.command);
    const auto rhs=commands::getCommandName(right.command);
    if(query.empty()||left.rank==0||left.rank==1)return lhs<rhs;
    if((left.rank==2||left.rank==3)&&lhs.size()!=rhs.size())
      return lhs.size()<rhs.size();
    return left.order<right.order;
  });
  std::vector<Suggestion> result;
  for(const auto& candidate:found){
    const auto name=commands::getCommandName(candidate.command);
    const auto alias=matchingAlias(candidate.command,query);
    Suggestion item;
    item.kind=Suggestion::Kind::Command;
    item.display="/"+name+(alias.empty()?"":" ("+alias+")");
    item.description=stringOr(value::property(candidate.command,"description"));
    item.replacement="/"+name+(span.atStart?" ":"");
    item.startUtf16=span.startUtf16;
    item.endUtf16=span.endUtf16;
    const auto& args=value::property(candidate.command,"argNames");
    item.executeWithoutArgs=span.atStart&&
      (value::property(candidate.command,"type")!="prompt"||
       !args.is_array()||args.empty());
    result.push_back(std::move(item));
    if(result.size()>=20)break;
  }
  return result;
}
} // namespace batbox::application_completion
