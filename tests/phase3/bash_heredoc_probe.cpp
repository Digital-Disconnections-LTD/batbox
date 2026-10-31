#include "shell/Heredoc.hpp"
#include "shell/CommandSplitter.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  const auto command=input.at("command").get<std::string>();
  const auto salt=[](){return std::string("0123456789abcdef");};
  const auto op=input.at("op").get<std::string>();
  Json result;
  if(op=="contains")result=batbox::shell_heredoc::containsHeredoc(command);
  else if(op=="extract"){
    const auto extracted=batbox::shell_heredoc::extractHeredocs(command,
      input.value("quotedOnly",false),salt);
    Json entries=Json::array();
    for(const auto& entry:extracted.heredocs){
      const auto& h=entry.info;
      entries.push_back(Json::array({entry.placeholder,{{"fullText",h.fullText},
        {"delimiter",h.delimiter},{"operatorStartIndex",h.operatorStartIndex},
        {"operatorEndIndex",h.operatorEndIndex},{"contentStartIndex",h.contentStartIndex},
        {"contentEndIndex",h.contentEndIndex}}}));
    }
    const auto parts=input.contains("parts")?input.at("parts").get<std::vector<std::string>>():
      std::vector<std::string>{extracted.processedCommand};
    result={{"processedCommand",extracted.processedCommand},{"heredocs",entries},
      {"restored",batbox::shell_heredoc::restoreHeredocs(parts,extracted.heredocs)}};
  }else if(op=="operators")result=batbox::shell_split::splitCommandWithOperators(command,salt);
  else if(op=="deprecated")result=batbox::shell_split::splitCommandDeprecated(command,salt);
  else throw std::logic_error("unknown op");
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){
  std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
}}
