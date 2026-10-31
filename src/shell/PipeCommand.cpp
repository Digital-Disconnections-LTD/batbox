#include "shell/PipeCommand.hpp"
#include "shell/ShellQuoteChecks.hpp"
#include "shell/CommandQuotingInternal.hpp"
#include "shell/ShellQuote.hpp"
#include "runtime/JsonValue.hpp"
#include <vector>

namespace batbox::shell_command {
namespace {
std::string singleQuoteForEval(const std::string& value){
  std::string result="'";
  for(char c:value)result+=c=='\''?"'\"'\"'":std::string(1,c);
  return result+"'";
}
std::string quoteWithEvalStdinRedirect(const std::string& command){
  return singleQuoteForEval(command)+" < /dev/null";
}
std::string joinContinuations(const std::string& command){
  std::string result;
  for(std::size_t i=0;i<command.size();){
    if(command[i]!='\\'){result+=command[i++];continue;}
    const auto start=i;
    while(i<command.size()&&command[i]=='\\')++i;
    const auto length=i-start;
    if(i<command.size()&&command[i]=='\n'&&length%2==1){
      result.append(length-1,'\\');++i;
    }else result.append(command,start,length);
  }
  return result;
}
bool containsVariableReference(const std::string& command){
  for(std::size_t i=0;i+1<command.size();++i)
    if(command[i]=='$'&&((command[i+1]>='A'&&command[i+1]<='Z')||
      (command[i+1]>='a'&&command[i+1]<='z')||command[i+1]=='_'||
      command[i+1]=='{'))return true;
  return false;
}
bool controlStructure(const std::string& command){
  const auto code=value::utf16CodeUnits(command);
  for(std::size_t i=0;i<code.size();++i){
    if(i>0&&detail::word(code[i-1]))continue;
    for(std::u16string_view keyword:{u"for",u"while",u"until",u"if",u"case",u"select"})
      if(i+keyword.size()<code.size()&&code.compare(i,keyword.size(),keyword)==0&&
         detail::whitespace(code[i+keyword.size()]))return true;
  }
  return false;
}
bool isOperator(const Json& entry,const std::string& op={}){
  if(!entry.is_object()||!entry.contains("op"))return false;
  return op.empty()||entry["op"]==op;
}
bool descriptor(const std::string& value){return value=="0"||value=="1"||value=="2";}
bool envAssignment(const std::string& value){
  if(value.empty()||!(value[0]>='A'&&value[0]<='Z')&&
     !(value[0]>='a'&&value[0]<='z')&&value[0]!='_')return false;
  std::size_t i=1;
  while(i<value.size()&&((value[i]>='A'&&value[i]<='Z')||
    (value[i]>='a'&&value[i]<='z')||(value[i]>='0'&&value[i]<='9')||value[i]=='_'))++i;
  return i<value.size()&&value[i]=='=';
}
bool separator(const std::string& op){return op=="&&"||op=="||"||op==";";}
std::string quoteOne(const std::string& value){return shell_quote::quote({Json(value)});}
std::vector<std::string> commandParts(const Json& parsed,std::size_t start,std::size_t end){
  std::vector<std::string> parts;
  bool seenNonEnvVar=false;
  for(std::size_t i=start;i<end;++i){
    const auto& entry=parsed[i];
    if(entry.is_string()&&descriptor(entry.get<std::string>())&&i+2<end&&isOperator(parsed[i+1])){
      const auto& op=parsed[i+1]["op"];
      const auto& target=parsed[i+2];
      if(op==">&"&&target.is_string()&&descriptor(target.get<std::string>())){
        parts.push_back(entry.get<std::string>()+">&"+target.get<std::string>());i+=2;continue;
      }
      if(op==">"&&target=="/dev/null"){
        parts.push_back(entry.get<std::string>()+">/dev/null");i+=2;continue;
      }
      if(op==">"&&target.is_string()){
        const auto value=target.get<std::string>();
        if(!value.empty()&&value[0]=='&'&&descriptor(value.substr(1))){
          parts.push_back(entry.get<std::string>()+">&"+value.substr(1));i+=2;continue;
        }
      }
    }
    if(entry.is_string()){
      const auto value=entry.get<std::string>();
      if(!seenNonEnvVar&&envAssignment(value)){
        const auto equals=value.find('=');
        parts.push_back(value.substr(0,equals)+"="+quoteOne(value.substr(equals+1)));
      }else{seenNonEnvVar=true;parts.push_back(quoteOne(value));}
    }else if(isOperator(entry)){
      if(entry["op"]=="glob"&&entry.contains("pattern"))
        parts.push_back(entry["pattern"].get<std::string>());
      else{
        const auto op=entry["op"].get<std::string>();
        parts.push_back(op);
        if(separator(op))seenNonEnvVar=false;
      }
    }
  }
  return parts;
}
}
std::string rearrangePipeCommand(const std::string& command){
  if(command.find('`')!=std::string::npos)return quoteWithEvalStdinRedirect(command);
  if(command.find("$(")!=std::string::npos)return quoteWithEvalStdinRedirect(command);
  if(containsVariableReference(command))return quoteWithEvalStdinRedirect(command);
  if(controlStructure(command))return quoteWithEvalStdinRedirect(command);
  const auto joined=joinContinuations(command);
  if(joined.find('\n')!=std::string::npos)return quoteWithEvalStdinRedirect(command);
  if(hasShellQuoteSingleQuoteBug(joined))return quoteWithEvalStdinRedirect(command);
  Json parsed;
  try{parsed=shell_quote::parse(joined);}
  catch(...){return quoteWithEvalStdinRedirect(command);}
  if(hasMalformedTokens(joined,parsed))return quoteWithEvalStdinRedirect(command);
  int firstPipe=-1;
  for(std::size_t i=0;i<parsed.size();++i)if(isOperator(parsed[i],"|")){
    firstPipe=static_cast<int>(i);break;}
  if(firstPipe<=0)return quoteWithEvalStdinRedirect(command);
  auto parts=commandParts(parsed,0,static_cast<std::size_t>(firstPipe));
  parts.push_back("< /dev/null");
  const auto rest=commandParts(parsed,static_cast<std::size_t>(firstPipe),parsed.size());
  parts.insert(parts.end(),rest.begin(),rest.end());
  std::string rebuilt;
  for(const auto& part:parts){if(!rebuilt.empty())rebuilt+=' ';rebuilt+=part;}
  return singleQuoteForEval(rebuilt);
}
} // namespace batbox::shell_command
