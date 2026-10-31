#include "termio/Output.hpp"
#include "termio/Csi.hpp"
#include "termio/Dec.hpp"
#include "termio/OscLink.hpp"
#include <memory>
namespace batbox::termio {
void writeDiffToTerminal(const std::function<void(const std::string&)>& write,
  const Json& diff,bool skipSyncMarkers,const OutputServices& services){
  if(diff.empty())return;
  const bool useSync=!skipSyncMarkers;
  std::string buffer=useSync?BSU:"";
  for(const auto& patch:diff){
    const auto type=patch.at("type").get<std::string>();
    if(type=="stdout")buffer+=patch.at("content").get<std::string>();
    else if(type=="clear"){
      const auto count=patch.at("count").get<double>();if(count>0)buffer+=eraseLines(count);
    }else if(type=="clearTerminal")buffer+=services.getClearTerminalSequence();
    else if(type=="cursorHide")buffer+=HIDE_CURSOR;
    else if(type=="cursorShow")buffer+=SHOW_CURSOR;
    else if(type=="cursorMove")buffer+=cursorMove(patch.at("x"),patch.at("y"));
    else if(type=="cursorTo")buffer+=cursorTo(patch.at("col"));
    else if(type=="carriageReturn")buffer+='\r';
    else if(type=="hyperlink")buffer+=services.link(patch.at("uri"));
    else if(type=="styleStr")buffer+=patch.at("str").get<std::string>();
  }
  if(useSync)buffer+=ESU;
  write(buffer);
}
OutputServices indigenousOutputServices(TerminalEnvironment env,
  std::function<std::string()> terminalName){
  OutputServices services;
  services.getClearTerminalSequence=[env=std::move(env)]{return getClearTerminalSequence(env);};
  auto module=std::make_shared<OscModule>(std::move(terminalName));
  services.link=[module=std::move(module)](const std::string& uri){return module->link(uri);};
  return services;
}
}
