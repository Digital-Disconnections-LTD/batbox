#include "terminal_hooks/Title.hpp"
#include <stdexcept>

namespace batbox::terminal_hooks {
void TerminalTitleEffect::commit(std::optional<std::string> title,
    WriteRawRef writeRaw,const TitleServices& services){
  if(mounted_&&title==previousTitle_&&writeRaw==previousWrite_)return;
  mounted_=true;
  previousTitle_=title;
  previousWrite_=writeRaw;
  if(!title||!writeRaw)return;
  if(!services.stripAnsi||!services.isWindows)
    throw std::logic_error("Terminal title imports required");
  const auto clean=services.stripAnsi(*title);
  if(services.isWindows()){
    if(!services.setProcessTitle)
      throw std::logic_error("Process title import required");
    services.setProcessTitle(clean);
  }else{
    if(!services.osc)throw std::logic_error("OSC import required");
    (*writeRaw)(services.osc(Json::array({0,clean})));
  }
}
}
