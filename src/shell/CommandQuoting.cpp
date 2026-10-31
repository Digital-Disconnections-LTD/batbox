#include "shell/CommandQuoting.hpp"
#include "shell/CommandQuotingInternal.hpp"
#include "shell/ShellQuote.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::shell_command {
namespace {
bool bitShift(const std::u16string& code){
  for(std::size_t i=0;i<code.size();++i){
    if(detail::digit(code[i])){
      auto j=i+1;while(j<code.size()&&detail::whitespace(code[j]))++j;
      if(j+1<code.size()&&code[j]==u'<'&&code[j+1]==u'<'){
        j+=2;while(j<code.size()&&detail::whitespace(code[j]))++j;
        if(j<code.size()&&detail::digit(code[j]))return true;
      }
    }
  }
  for(std::size_t i=0;(i=code.find(u"$((",i))!=std::u16string::npos;++i){
    const auto end=code.find(u"))",i+3);
    if(end==std::u16string::npos)continue;
    const auto lineEnd=code.find(u'\n',i+3);
    if(lineEnd!=std::u16string::npos&&lineEnd<end)continue;
    if(code.find(u"<<",i+3)<end)return true;
  }
  return false;
}
bool heredocPattern(const std::u16string& code){
  for(std::size_t i=0;(i=code.find(u"<<",i))!=std::u16string::npos;++i){
    std::size_t j=i+2;
    if(j<code.size()&&code[j]==u'-')++j;
    while(j<code.size()&&detail::whitespace(code[j]))++j;
    if(j<code.size()&&code[j]==u'\\'){
      ++j;const auto begin=j;
      while(j<code.size()&&detail::word(code[j]))++j;
      if(j>begin)return true;
      continue;
    }
    const auto quote=j<code.size()&&(code[j]==u'\''||code[j]==u'"')?code[j++]:0;
    const auto begin=j;
    while(j<code.size()&&detail::word(code[j]))++j;
    if(j>begin&&(!quote||j<code.size()&&code[j]==quote))return true;
  }
  return false;
}
}
bool containsHeredocForQuoting(const std::string& command){
  const auto code=value::utf16CodeUnits(command);
  if(bitShift(code))return false;
  return heredocPattern(code);
}
bool containsMultilineString(const std::string& command){
  const auto code=value::utf16CodeUnits(command);
  return detail::multilineQuoted(code,u'\'')||detail::multilineQuoted(code,u'"');
}
std::string quoteShellCommand(const std::string& command,bool addStdinRedirect){
  if(containsHeredocForQuoting(command)||containsMultilineString(command)){
    std::string escaped;escaped.reserve(command.size()+4);
    for(char c:command)escaped+=c=='\''?"'\"'\"'":std::string(1,c);
    const auto quoted="'"+escaped+"'";
    if(containsHeredocForQuoting(command))return quoted;
    return addStdinRedirect?quoted+" < /dev/null":quoted;
  }
  Json items=Json::array({command});
  if(addStdinRedirect){items.push_back("<");items.push_back("/dev/null");}
  return shell_quote::quote(items.get<std::vector<Json>>());
}
} // namespace batbox::shell_command
