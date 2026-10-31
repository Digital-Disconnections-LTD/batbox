#include "cleanup/Terminal.hpp"
#include <stdexcept>

namespace batbox::cleanup {
namespace {
void require(const TerminalServices& services){
  if(!services.stdoutIsTTY||!services.isInteractive||
    !services.isFlightPersistenceDisabled||!services.supportsTabStatus||
    !services.disableTerminalTitle||!services.isWindows||
    !services.getInkInstance||!services.getFlightId||!services.flightIdExists||
    !services.getCurrentFlightTitle||!services.dim||!services.wrapForMultiplexer||
    !services.writeStdout||!services.setProcessTitle)
    throw std::invalid_argument("terminal shutdown imports required");
}
}
TerminalLifecycle::TerminalLifecycle(TerminalServices services):services_(std::move(services)){
  require(services_);
}
void TerminalLifecycle::cleanupTerminalModes(){
  if(!services_.stdoutIsTTY())return;
  try{
    const auto& seq=services_.sequences;
    services_.writeStdout(seq.disableMouseTracking);
    const auto instance=services_.getInkInstance();
    if(instance&&instance->isAltScreenActive){
      try{instance->unmount();}
      catch(...){services_.writeStdout(seq.exitAltScreen);}
    }
    if(instance){instance->drainStdin();instance->detachForShutdown();}
    services_.writeStdout(seq.disableModifyOtherKeys);
    services_.writeStdout(seq.disableKittyKeyboard);
    services_.writeStdout(seq.disableFocusEvents);
    services_.writeStdout(seq.disableBracketedPaste);
    services_.writeStdout(seq.showCursor);
    services_.writeStdout(seq.clearProgress);
    if(services_.supportsTabStatus())
      services_.writeStdout(services_.wrapForMultiplexer(seq.clearTabStatus));
    if(!services_.disableTerminalTitle()){
      if(services_.isWindows())services_.setProcessTitle("");
      else services_.writeStdout(seq.clearTitle);
    }
  }catch(...){} // Ignore errors from a dead terminal.
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
void TerminalLifecycle::resetResumeHint(){resumeHintPrinted_=false;}
} // namespace batbox::cleanup
