#include "glob/PicomatchSafety.hpp"
#include "glob/PicomatchParser.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <optional>
#include <vector>

namespace batbox::glob::detail {
namespace {
std::u16string trimmed(std::u16string_view value){
  return batbox::value::utf16CodeUnits(batbox::value::trim(batbox::value::fromUtf16(value)));
}
std::vector<std::u16string> splitTopLevel(std::u16string_view input){
  std::vector<std::u16string> parts;std::u16string value;
  int bracket=0,paren=0,quote=0;bool escaped=false;
  for(const auto ch:input){
    if(escaped){value+=ch;escaped=false;continue;}
    if(ch==u'\\'){value+=ch;escaped=true;continue;}
    if(ch==u'"'){quote=quote?0:1;value+=ch;continue;}
    if(!quote){
      if(ch==u'[')bracket++;
      else if(ch==u']'&&bracket>0)bracket--;
      else if(!bracket){
        if(ch==u'(')paren++;
        else if(ch==u')'&&paren>0)paren--;
        else if(ch==u'|'&&!paren){parts.push_back(value);value.clear();continue;}
      }
    }
    value+=ch;
  }
  parts.push_back(value);return parts;
}
bool isPlainBranch(std::u16string_view branch){
  bool escaped=false;
  for(const auto ch:branch){
    if(escaped){escaped=false;continue;}
    if(ch==u'\\'){escaped=true;continue;}
    if(ch==u'?'||ch==u'*'||ch==u'+'||ch==u'@'||ch==u'!'||
      ch==u'('||ch==u')'||ch==u'['||ch==u']'||ch==u'{'||ch==u'}')
      return false;
  }
  return true;
}
std::optional<std::u16string> normalizeSimpleBranch(std::u16string_view input){
  auto value=trimmed(input);
  while(value.size()>=4&&value.starts_with(u"@(")&&value.back()==u')'){
    const auto body=value.substr(2,value.size()-3);
    if(body.empty()||body.find_first_of(u"\\()[]{}|")!=std::u16string::npos)break;
    value=body;
  }
  if(!isPlainBranch(value))return std::nullopt;
  std::u16string output;
  for(std::size_t i=0;i<value.size();i++){
    if(value[i]==u'\\'&&i+1<value.size())i++;
    output+=value[i];
  }
  return output;
}
bool repeatedCharPrefixOverlap(const std::vector<std::u16string>& branches){
  std::vector<std::u16string> values;
  for(const auto& branch:branches){
    auto normalized=normalizeSimpleBranch(branch);
    if(normalized&&!normalized->empty())values.push_back(std::move(*normalized));
  }
  for(std::size_t i=0;i<values.size();i++)for(std::size_t j=i+1;j<values.size();j++){
    const auto& a=values[i];const auto& b=values[j];
    if(a.empty()||b.empty())continue;
    const auto ch=a[0];
    if(!std::all_of(a.begin(),a.end(),[ch](auto x){return x==ch;})||
      !std::all_of(b.begin(),b.end(),[ch](auto x){return x==ch;}))continue;
    if(a==b||a.starts_with(b)||b.starts_with(a))return true;
  }
  return false;
}
struct RepeatedExtglob {char16_t type;std::u16string body;std::size_t end;};
std::optional<RepeatedExtglob> parseRepeatedExtglob(
  std::u16string_view pattern,bool requireEnd=true){
  if(pattern.size()<3||(pattern[0]!=u'+'&&pattern[0]!=u'*')||pattern[1]!=u'(')
    return std::nullopt;
  int bracket=0,paren=0,quote=0;bool escaped=false;
  for(std::size_t i=1;i<pattern.size();i++){
    const auto ch=pattern[i];
    if(escaped){escaped=false;continue;}
    if(ch==u'\\'){escaped=true;continue;}
    if(ch==u'"'){quote=quote?0:1;continue;}
    if(quote)continue;
    if(ch==u'['){bracket++;continue;}
    if(ch==u']'&&bracket>0){bracket--;continue;}
    if(bracket>0)continue;
    if(ch==u'('){paren++;continue;}
    if(ch==u')'){
      paren--;
      if(paren==0){
        if(requireEnd&&i!=pattern.size()-1)return std::nullopt;
        return RepeatedExtglob{pattern[0],std::u16string(pattern.substr(2,i-2)),i};
      }
    }
  }
  return std::nullopt;
}
std::optional<std::u16string> starExtglobSequenceOutput(std::u16string_view pattern){
  std::size_t index=0;std::vector<char16_t> chars;
  while(index<pattern.size()){
    auto match=parseRepeatedExtglob(pattern.substr(index),false);
    if(!match||match->type!=u'*')return std::nullopt;
    auto branches=splitTopLevel(match->body);
    if(branches.size()!=1)return std::nullopt;
    auto branch=normalizeSimpleBranch(trimmed(branches[0]));
    if(!branch||branch->size()!=1)return std::nullopt;
    chars.push_back((*branch)[0]);index+=match->end+1;
  }
  if(chars.empty())return std::nullopt;
  if(chars.size()==1)return escapePicoRegex(std::u16string_view(&chars[0],1))+u"*";
  std::u16string output=u"[";
  for(auto ch:chars)output+=escapePicoRegex(std::u16string_view(&ch,1));
  return output+u"]*";
}
int repeatedExtglobRecursion(std::u16string_view pattern){
  int depth=0;auto value=trimmed(pattern);
  auto match=parseRepeatedExtglob(value);
  while(match){
    depth++;value=trimmed(match->body);
    match=parseRepeatedExtglob(value);
  }
  return depth;
}
}
ExtglobAnalysis analyzeRepeatedExtglob(std::u16string_view body){
  auto branches=splitTopLevel(body);
  for(auto& branch:branches)branch=trimmed(branch);
  if(branches.size()>1){
    if(std::any_of(branches.begin(),branches.end(),[](const auto& x){return x.empty();})||
      std::any_of(branches.begin(),branches.end(),[](const auto& x){
        return !x.empty()&&std::all_of(x.begin(),x.end(),[](auto c){return c==u'*'||c==u'?';});})||
      repeatedCharPrefixOverlap(branches))return {true,std::nullopt};
  }
  for(const auto& branch:branches){
    if(auto safe=starExtglobSequenceOutput(branch))return {true,std::move(safe)};
    if(repeatedExtglobRecursion(branch)>0)return {true,std::nullopt};
  }
  return {};
}
} // namespace batbox::glob::detail
