#include "shell/HeredocInternal.hpp"

namespace batbox::shell_heredoc::detail {
namespace {
bool word(char16_t c){return (c>=u'A'&&c<=u'Z')||(c>=u'a'&&c<=u'z')||
  (c>=u'0'&&c<=u'9')||c==u'_';}
bool eofMetachar(char16_t c){
  return c==u')'||c==u'}'||c==u'`'||c==u'|'||c==u'&'||c==u';'||
    c==u'('||c==u'<'||c==u'>';
}
}
std::vector<Match> findStarts(const std::u16string& command){
  std::vector<Match> matches;
  for(std::size_t i=0;i+1<command.size();++i){
    if(command[i]!=u'<'||command[i+1]!=u'<'||
       (i>0&&command[i-1]==u'<')||
       (i+2<command.size()&&command[i+2]==u'<'))continue;
    std::size_t p=i+2;
    bool dash=p<command.size()&&command[p]==u'-';if(dash)++p;
    while(p<command.size()&&(command[p]==u' '||command[p]==u'\t'))++p;
    char16_t quote=0;
    if(p<command.size()&&(command[p]==u'\''||command[p]==u'"')){
      quote=command[p++];const auto begin=p;
      if(p<command.size()&&command[p]==u'\\')++p;
      const auto wordBegin=p;
      while(p<command.size()&&word(command[p]))++p;
      if(wordBegin==p||p>=command.size()||command[p]!=quote)continue;
      matches.push_back({i,p+1,dash,quote,command.substr(begin,p-begin)});
      i=p;
    }else{
      if(p<command.size()&&command[p]==u'\\')++p;
      const auto begin=p;
      while(p<command.size()&&word(command[p]))++p;
      if(begin==p)continue;
      matches.push_back({i,p,dash,0,command.substr(begin,p-begin)});
      i=p-1;
    }
  }
  return matches;
}
std::size_t logicalNewline(const std::u16string& command,std::size_t from){
  bool single=false,dbl=false;
  for(std::size_t k=from;k<command.size();++k){
    const auto ch=command[k];
    if(single){if(ch==u'\'')single=false;continue;}
    if(dbl){
      if(ch==u'\\'){++k;continue;}
      if(ch==u'"')dbl=false;
      continue;
    }
    if(ch==u'\n')return k;
    std::size_t backslashes=0;
    for(std::size_t j=k;j>from&&command[j-1]==u'\\';--j)++backslashes;
    if(backslashes%2==1)continue;
    if(ch==u'\'')single=true;
    else if(ch==u'"')dbl=true;
  }
  return std::u16string::npos;
}
int closingLine(const std::u16string& command,std::size_t contentStart,
  const std::u16string& delimiter,bool dash,std::size_t& contentEnd){
  std::size_t lineStart=contentStart+1;
  int lineIndex=0;
  while(lineStart<=command.size()){
    const auto end=command.find(u'\n',lineStart);
    const auto lineEnd=end==std::u16string::npos?command.size():end;
    auto checkStart=lineStart;
    if(dash)while(checkStart<lineEnd&&command[checkStart]==u'\t')++checkStart;
    const auto length=lineEnd-checkStart;
    if(length==delimiter.size()&&command.compare(checkStart,length,delimiter)==0){
      contentEnd=lineEnd;
      return lineIndex;
    }
    if(length>delimiter.size()&&command.compare(checkStart,delimiter.size(),delimiter)==0&&
       eofMetachar(command[checkStart+delimiter.size()]))return -1;
    if(end==std::u16string::npos)break;
    lineStart=end+1;++lineIndex;
  }
  return -1;
}
} // namespace batbox::shell_heredoc::detail
