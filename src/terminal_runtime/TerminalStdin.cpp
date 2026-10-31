#include "terminal_runtime/TerminalControls.hpp"
#include <array>

namespace batbox::terminal_runtime {
void drainStdin(InputPort& stdin,const DrainServices& services){
  if(!stdin.isTTY())return;
  try{while(stdin.read().has_value()){} }
  catch(...){/* Ignore an already destroyed stream. */}
  if(services.windows)return;
  const bool wasRaw=stdin.isRaw();
  int fd=-1;
  try{
    if(!wasRaw&&stdin.hasSetRawMode())stdin.setRawMode(true);
    fd=services.openNonblockingTty();
    std::array<char,1024> buffer{};
    for(int i=0;i<64;++i)
      if(services.readTty(fd,buffer.data(),buffer.size())<=0)break;
  }catch(...){/* EAGAIN, missing/revoked TTY, or raw-mode failure. */}
  if(fd>=0){try{services.closeTty(fd);}catch(...){} }
  if(!wasRaw&&stdin.hasSetRawMode()){
    try{stdin.setRawMode(false);}catch(...){}
  }
}
void TerminalControls::drainStdin(){terminal_runtime::drainStdin(stdin_,services_.drain);}
void TerminalControls::suspendStdin(){
  if(!stdin_.isTTY())return;
  const auto readable=stdin_.listeners("readable");
  services_.logForDebugging(
    "[stdin] suspendStdin: removing "+std::to_string(readable.size())+
    " readable listener(s), wasRawMode="+
    std::string(stdin_.isRaw()?"true":"false"),"");
  for(const auto& listener:readable){
    stdinListeners_.push_back({"readable",listener});
    stdin_.removeListener("readable",listener);
  }
  if(stdin_.isRaw()&&stdin_.hasSetRawMode()){
    stdin_.setRawMode(false);
    wasRawMode_=true;
  }
}
void TerminalControls::resumeStdin(){
  if(!stdin_.isTTY())return;
  if(stdinListeners_.empty()&&!wasRawMode_)
    services_.logForDebugging(
      "[stdin] resumeStdin: called with no stored listeners and "
      "wasRawMode=false (possible desync)","warn");
  services_.logForDebugging(
    "[stdin] resumeStdin: re-attaching "+
    std::to_string(stdinListeners_.size())+
    " listener(s), wasRawMode="+
    std::string(wasRawMode_?"true":"false"),"");
  for(const auto& saved:stdinListeners_)
    stdin_.addListener(saved.event,saved.listener);
  stdinListeners_.clear();
  if(wasRawMode_){
    if(stdin_.hasSetRawMode())stdin_.setRawMode(true);
    wasRawMode_=false;
  }
}
void TerminalControls::detachForShutdown(){
  isUnmounted_=true;
  if(services_.cancelScheduledRender)services_.cancelScheduledRender();
  drainStdin();
  if(stdin_.isTTY()&&stdin_.isRaw()&&stdin_.hasSetRawMode())
    stdin_.setRawMode(false);
}
} // namespace batbox::terminal_runtime
