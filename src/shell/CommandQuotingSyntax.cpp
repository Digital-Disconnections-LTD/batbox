#include "shell/CommandQuotingInternal.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::shell_command::detail {
bool whitespace(char16_t c){return value::trim(value::fromUtf16(std::u16string_view(&c,1))).empty();}
bool digit(char16_t c){return c>=u'0'&&c<=u'9';}
bool word(char16_t c){return digit(c)||(c>=u'a'&&c<=u'z')||(c>=u'A'&&c<=u'Z')||c==u'_';}
bool newline(char16_t c){return c==u'\n'||c==u'\r'||c==u'\u2028'||c==u'\u2029';}
bool multilineQuoted(const std::u16string& code,char16_t quote){
  for(std::size_t i=0;i<code.size();++i){
    if(code[i]!=quote)continue;
    bool seenNewline=false;
    for(std::size_t j=i+1;j<code.size();++j){
      if(code[j]==u'\\'){
        if(j+1>=code.size()||newline(code[j+1]))break;
        ++j;continue;
      }
      if(code[j]==quote){if(seenNewline)return true;break;}
      if(code[j]==u'\n')seenNewline=true;
    }
  }
  return false;
}
} // namespace batbox::shell_command::detail
