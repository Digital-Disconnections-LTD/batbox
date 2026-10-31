#include "shell/HeredocInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <utility>

namespace batbox::shell_heredoc::detail {
namespace {
bool terminator(char16_t c){
  return c==u' '||c==u'\t'||c==u'\n'||c==u'|'||c==u'&'||
    c==u';'||c==u'('||c==u')'||c==u'<'||c==u'>';
}
std::size_t count(const std::u16string& s,std::u16string_view pattern){
  std::size_t result=0,pos=0;
  while((pos=s.find(pattern,pos))!=std::u16string::npos){++result;pos+=pattern.size();}
  return result;
}
std::string utf8(const std::u16string& s,std::size_t begin,std::size_t end){
  return value::fromUtf16(std::u16string_view(s).substr(begin,end-begin));
}
struct Range {std::size_t begin{},end{};};
struct Scanner {
  const std::u16string& command;
  std::size_t pos=0,pendingBackslashes=0;
  bool single=false,dbl=false,comment=false,dqEscapeNext=false;
  void advance(std::size_t target){
    for(std::size_t i=pos;i<target;++i){
      const auto ch=command[i];
      if(ch==u'\n')comment=false;
      if(single){if(ch==u'\'')single=false;continue;}
      if(dbl){
        if(dqEscapeNext){dqEscapeNext=false;continue;}
        if(ch==u'\\'){dqEscapeNext=true;continue;}
        if(ch==u'"')dbl=false;
        continue;
      }
      if(ch==u'\\'){++pendingBackslashes;continue;}
      const bool escaped=pendingBackslashes%2==1;
      pendingBackslashes=0;
      if(escaped)continue;
      if(ch==u'\'')single=true;
      else if(ch==u'"')dbl=true;
      else if(!comment&&ch==u'#')comment=true;
    }
    pos=target;
  }
};
}
std::vector<HeredocInfo> collect(const std::u16string& command,bool quotedOnly){
  std::vector<HeredocInfo> matches;
  const auto first=command.find(u"<<");
  if(first==std::u16string::npos)return matches;
  for(std::size_t i=0;i+1<command.size();++i)
    if(command[i]==u'$'&&(command[i+1]==u'\''||command[i+1]==u'"'))return matches;
  if(first>0&&command.substr(0,first).find(u'`')!=std::u16string::npos)return matches;
  if(first>0){
    const auto before=command.substr(0,first);
    if(count(before,u"((")>count(before,u"))"))return matches;
  }
  std::vector<Range> skipped;
  Scanner scanner{command};
  for(const auto& match:findStarts(command)){
    scanner.advance(match.start);
    if(scanner.single||scanner.dbl||scanner.comment||scanner.pendingBackslashes%2==1)continue;
    bool insideSkipped=false;
    for(const auto& range:skipped)
      if(match.start>range.begin&&match.start<range.end){insideSkipped=true;break;}
    if(insideSkipped)continue;
    if(match.end<command.size()&&!terminator(command[match.end]))continue;
    const auto newline=logicalNewline(command,match.end);
    if(newline==std::u16string::npos)continue;
    std::size_t trailing=0;
    for(auto j=newline;j>match.end&&command[j-1]==u'\\';--j)++trailing;
    if(trailing%2==1)continue;
    std::size_t contentEnd=0;
    const auto closing=closingLine(command,newline,match.delimiter,match.dash,contentEnd);
    const auto operatorText=command.substr(match.start,match.end-match.start);
    const bool quoted=match.quote!=0||operatorText.find(u'\\')!=std::u16string::npos;
    if(quotedOnly&&!quoted){
      skipped.push_back({newline,closing<0?command.size():contentEnd});
      continue;
    }
    if(closing<0)continue;
    bool overlaps=false;
    for(const auto& range:skipped)
      if(newline<range.end&&range.begin<contentEnd){overlaps=true;break;}
    if(overlaps)continue;
    matches.push_back({
      utf8(command,match.start,match.end)+utf8(command,newline,contentEnd),
      value::fromUtf16(match.delimiter),match.start,match.end,newline,contentEnd});
  }
  return matches;
}
} // namespace batbox::shell_heredoc::detail
