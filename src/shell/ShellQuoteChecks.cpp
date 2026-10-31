#include "shell/ShellQuoteChecks.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::shell_command {
namespace {
std::size_t count(const std::string& value,char c){
  std::size_t total=0;for(char x:value)if(x==c)++total;return total;
}
std::size_t unescapedQuotes(const std::string& value,char quote){
  std::size_t total=0;
  for(std::size_t i=0;i<value.size();++i)
    if(value[i]==quote&&(i==0||value[i-1]!='\\'))++total;
  return total;
}
}
bool hasMalformedTokens(const std::string& command,const Json& parsed){
  const auto code=value::utf16CodeUnits(command);
  bool inSingle=false,inDouble=false;
  std::size_t doubleCount=0,singleCount=0;
  for(std::size_t i=0;i<code.size();++i){
    const auto c=code[i];
    if(c==u'\\'&&!inSingle){++i;continue;}
    if(c==u'"'&&!inSingle){++doubleCount;inDouble=!inDouble;}
    else if(c==u'\''&&!inDouble){++singleCount;inSingle=!inSingle;}
  }
  if(doubleCount%2||singleCount%2)return true;
  for(const auto& entry:parsed){
    if(!entry.is_string())continue;
    const auto value=entry.get<std::string>();
    if(count(value,'{')!=count(value,'}')||
       count(value,'(')!=count(value,')')||
       count(value,'[')!=count(value,']'))return true;
    if(unescapedQuotes(value,'"')%2||unescapedQuotes(value,'\'')%2)return true;
  }
  return false;
}
bool hasShellQuoteSingleQuoteBug(const std::string& command){
  const auto code=value::utf16CodeUnits(command);
  bool single=false,dbl=false;
  for(std::size_t i=0;i<code.size();++i){
    const auto c=code[i];
    if(c==u'\\'&&!single){++i;continue;}
    if(c==u'"'&&!single){dbl=!dbl;continue;}
    if(c==u'\''&&!dbl){
      single=!single;
      if(!single){
        std::size_t backslashes=0;
        for(std::size_t j=i;j>0&&code[j-1]==u'\\';--j)++backslashes;
        if(backslashes>0&&backslashes%2==1)return true;
        if(backslashes>0&&backslashes%2==0&&code.find(u'\'',i+1)!=std::u16string::npos)
          return true;
      }
    }
  }
  return false;
}
} // namespace batbox::shell_command
