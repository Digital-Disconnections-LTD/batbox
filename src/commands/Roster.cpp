#include "commands/Roster.hpp"

namespace batbox::commands {
std::optional<ParsedSlashCommand> parseSlashCommand(const std::string& input){
  const auto trimmed=value::trim(input);
  if(trimmed.empty()||trimmed[0]!='/')return std::nullopt;
  const auto without=value::slice(trimmed,1);
  std::vector<std::string> words;
  std::size_t start=0;
  while(true){
    const auto space=without.find(' ',start);
    words.emplace_back(without.substr(start,space==std::string::npos?space:space-start));
    if(space==std::string::npos)break;
    start=space+1;
  }
  if(words[0].empty())return std::nullopt;
  ParsedSlashCommand result{words[0],"",false};
  std::size_t argsStart=1;
  if(words.size()>1&&words[1]=="(MCP)"){
    result.commandName+=" (MCP)";
    result.isMcp=true;
    argsStart=2;
  }
  for(std::size_t i=argsStart;i<words.size();++i){
    if(i>argsStart)result.args+=' ';
    result.args+=words[i];
  }
  return result;
}
bool looksLikeCommand(const std::string& name){
  return std::all_of(name.begin(),name.end(),[](unsigned char c){
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
      (c>='0'&&c<='9')||c==':'||c=='-'||c=='_';
  });
}
std::string getCommandName(const Json& command){
  const auto& display=value::property(command,"userFacingName");
  if(display.is_string())return display.get<std::string>();
  const auto& name=value::property(command,"name");
  return name.is_string()?name.get<std::string>():std::string{};
}
} // namespace batbox::commands
