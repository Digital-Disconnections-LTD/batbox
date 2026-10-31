#include "shell/CommandQuoting.hpp"
#include "shell/CommandQuotingInternal.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::shell_command {
bool hasStdinRedirect(const std::string& command){
  const auto code=value::utf16CodeUnits(command);
  for(std::size_t i=0;i<code.size();++i){
    if(code[i]!=u'<'||i>0&&!detail::whitespace(code[i-1])&&
       code[i-1]!=u';'&&code[i-1]!=u'&'&&code[i-1]!=u'|')continue;
    if(i+1<code.size()&&(code[i+1]==u'<'||code[i+1]==u'('))continue;
    auto j=i+1;
    while(j<code.size()&&detail::whitespace(code[j]))++j;
    if(j<code.size()&&!detail::whitespace(code[j]))return true;
  }
  return false;
}
bool shouldAddStdinRedirect(const std::string& command){
  if(containsHeredocForQuoting(command))return false;
  if(hasStdinRedirect(command))return false;
  return true;
}
std::string rewriteWindowsNullRedirect(const std::string& command){
  const auto code=value::utf16CodeUnits(command);
  std::u16string result;result.reserve(code.size());
  for(std::size_t i=0;i<code.size();){
    std::size_t j=i;
    if(j<code.size()&&detail::digit(code[j]))++j;
    if(j<code.size()&&code[j]==u'&')++j;
    const auto gtBegin=j;
    while(j<code.size()&&code[j]==u'>')++j;
    if(j==gtBegin){result+=code[i++];continue;}
    while(j<code.size()&&detail::whitespace(code[j]))++j;
    if(j+2>=code.size()||
       (code[j]!=u'n'&&code[j]!=u'N')||
       (code[j+1]!=u'u'&&code[j+1]!=u'U')||
       (code[j+2]!=u'l'&&code[j+2]!=u'L')){
      result+=code[i++];continue;
    }
    const auto after=j+3;
    if(after<code.size()&&!detail::whitespace(code[after])&&
       code[after]!=u'|'&&code[after]!=u'&'&&code[after]!=u';'&&
       code[after]!=u')'&&code[after]!=u'\n'){
      result+=code[i++];continue;
    }
    result.append(code,i,j-i);result+=u"/dev/null";i=after;
  }
  return value::fromUtf16(result);
}
} // namespace batbox::shell_command
