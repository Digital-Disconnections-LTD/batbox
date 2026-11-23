#include "cleanup/Terminal.hpp"
#include <stdexcept>

namespace batbox::cleanup {
namespace {
void require(const TerminalServices& services){
  if(!services.stdoutIsTTY||!services.isInteractive||
    !services.isFlightPersistenceDisabled||!services.getFlightId||
    !services.flightIdExists||!services.getCurrentFlightTitle||
    !services.dim||!services.writeStdout)
    throw std::invalid_argument("terminal shutdown imports required");
}
}
TerminalLifecycle::TerminalLifecycle(TerminalServices services):services_(std::move(services)){
  require(services_);
}
void TerminalLifecycle::printResumeHint(){
  if(resumeHintPrinted_)return;
  if(!(services_.stdoutIsTTY()&&services_.isInteractive()&&
    !services_.isFlightPersistenceDisabled()))return;
  try{
    const auto id=services_.getFlightId();
    if(!services_.flightIdExists(id))return;
    const auto title=services_.getCurrentFlightTitle(id);
    std::string argument=id;
    if(title&&!title->empty()){
      argument="\"";
      for(const char ch:*title){
        if(ch=='\\'||ch=='\"')argument+='\\';
        argument+=ch;
      }
      argument+='\"';
    }
    services_.writeStdout(services_.dim("\nResume this session with:\n"
      "batbox --resume "+argument+"\n"));
    resumeHintPrinted_=true;
  }catch(...){} // Ignore closed-output and Flight lookup errors.
}
} // namespace batbox::cleanup
