#include "cleanup/Terminal.hpp"
#include "runtime/Encoding.hpp"
#include <algorithm>
#include <cerrno>
#ifndef _WIN32
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <stdexcept>
#include <string>
#include <system_error>

namespace batbox::cleanup {
TerminalSequences indigenousTerminalSequences(bool kittyTerminal){
  const std::string tab="\x1b]21337;indicator=;status=;status-color="+
    std::string(kittyTerminal?"\x1b\\":"\x07");
  return {
    "\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l",
    "\x1b[?1049l","\x1b[>4m","\x1b[<u","\x1b[?1004l",
    "\x1b[?2004l","\x1b[?25h","\x1b]9;4;0;\x07",tab,"\x1b]0;\x07"
  };
}
std::string indigenousWrapForMultiplexer(const std::string& sequence,bool tmux,bool screen){
  if(tmux){
    std::string escaped;
    for(const char ch:sequence){if(ch=='\x1b')escaped+='\x1b';escaped+=ch;}
    return "\x1bPtmux;"+escaped+"\x1b\\";
  }
  if(screen)return "\x1bP"+sequence+"\x1b\\";
  return sequence;
}
void indigenousWriteStdout(const std::string& text){
  const auto encoded=value::encodeUtf8(text);
#ifdef _WIN32
  const HANDLE output=::GetStdHandle(STD_OUTPUT_HANDLE);
  if(output==INVALID_HANDLE_VALUE||output==nullptr)
    throw std::system_error(static_cast<int>(::GetLastError()),
      std::system_category(),"stdout handle");
  for(std::size_t offset=0;offset<encoded.size();){
    const auto wanted=static_cast<DWORD>(std::min<std::size_t>(
      encoded.size()-offset,0xffffffffu));
    DWORD written=0;
    if(!::WriteFile(output,encoded.data()+offset,wanted,&written,nullptr))
      throw std::system_error(static_cast<int>(::GetLastError()),
        std::system_category(),"write stdout");
    if(written==0)throw std::runtime_error("write stdout returned zero");
    offset+=written;
  }
#else
  // Keep SIGPIPE local to the calling thread so terminal cleanup can handle
  // a closed pipe without terminating the process.
  sigset_t blocked{},previous{};
  sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
  const int maskError=pthread_sigmask(SIG_BLOCK,&blocked,&previous);
  if(maskError)throw std::system_error(maskError,std::generic_category(),"block SIGPIPE");
  struct Restore {
    sigset_t previous;
    ~Restore(){pthread_sigmask(SIG_SETMASK,&previous,nullptr);}
  } restore{previous};
  sigset_t pending{};sigpending(&pending);
  const bool alreadyPending=sigismember(&pending,SIGPIPE)==1;
  std::size_t offset=0;
  while(offset<encoded.size()){
    const auto written=::write(STDOUT_FILENO,encoded.data()+offset,encoded.size()-offset);
    if(written<0){if(errno==EINTR)continue;
      const int writeError=errno;
      if(writeError==EPIPE&&!alreadyPending&&sigismember(&previous,SIGPIPE)!=1){
#ifdef __APPLE__
        // macOS has no sigtimedwait; do not wait when SIGPIPE was ignored.
        if(sigpending(&pending)==0&&sigismember(&pending,SIGPIPE)==1){
          int signal=0;(void)sigwait(&blocked,&signal);
        }
#else
        timespec immediate{};(void)sigtimedwait(&blocked,nullptr,&immediate);
#endif
      }
      throw std::system_error(writeError,std::generic_category(),"write stdout");}
    if(written==0)throw std::runtime_error("write stdout returned zero");
    offset+=static_cast<std::size_t>(written);
  }
#endif
}
} // namespace batbox::cleanup
