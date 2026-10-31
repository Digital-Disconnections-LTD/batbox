#include "terminal_ftxui/Driver.hpp"
#include <ftxui/dom/elements.hpp>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <signal.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include <stdexcept>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

using batbox::terminal_ftxui::Driver;
using batbox::terminal_ftxui::Callbacks;
using batbox::terminal_ftxui::Options;
using batbox::event_loop::Loop;

static bool cooked(){
  termios state{};
  if(tcgetattr(STDIN_FILENO,&state)!=0)return false;
  return (state.c_lflag&(ECHO|ICANON))==(ECHO|ICANON);
}
static bool raw(){
  termios state{};
  if(tcgetattr(STDIN_FILENO,&state)!=0)return false;
  return (state.c_lflag&(ECHO|ICANON))==0;
}
static void child(int reportFd){
  bool before=cooked(),during=false,restored=false,reinstalled=false;
  bool restoredAfterThrow=false,suspended=false,resumed=false;
  bool fullscreen=false,inlineAgain=false,after=false,ownerThread=true;
  int typedEvents=0,exits=0,rendered=0,committed=0;
  std::string input;
  try{
    Loop owner;
    auto driver=std::make_shared<Driver>(owner,Options{},Callbacks{
      .render=[&]{++rendered;return ftxui::text("Batbox PTY probe");},
      .onRawInputBytes=[&](std::string_view bytes){
        ownerThread=ownerThread&&owner.isOwnerThread();
        during=raw();input.append(bytes);
      },
      .onEvent=[&](const ftxui::Event&){++typedEvents;},
      .onResize=[](ftxui::Dimensions){},
      .renderPlain={},
      .writePlain={},
      .onExit=[&]{++exits;},
      .onFrameCommitted=[&]{++committed;ownerThread=ownerThread&&owner.isOwnerThread();},
    });
    driver->start();
    auto inspection=owner.setTimeout(350,[&]{
      driver->withRestoredIO([&]{restored=cooked();});
      reinstalled=raw();
      try{
        driver->withRestoredIO([]{throw std::runtime_error("external failed");});
      }catch(const std::runtime_error& error){
        restoredAfterThrow=std::string(error.what())=="external failed"&&raw();
      }
      driver->suspend();suspended=cooked();
      driver->resume();resumed=raw();
      driver->setMode(batbox::terminal_ftxui::Mode::Fullscreen);
      fullscreen=driver->mode()==batbox::terminal_ftxui::Mode::Fullscreen&&raw();
      driver->setMode(batbox::terminal_ftxui::Mode::Inline);
      inlineAgain=driver->mode()==batbox::terminal_ftxui::Mode::Inline&&raw();
      driver->stop();after=cooked();
    });
    auto deadline=owner.setTimeout(1800,[&]{driver->stop();});
    owner.run();
    inspection.cancel();deadline.cancel();
  }catch(const std::exception& error){
    std::string message=std::string("ERR ")+error.what()+"\n";
    (void)!write(reportFd,message.data(),message.size());
    _exit(2);
  }
  const std::string expected="x\x1b[?1;2c";
  bool ok=before&&during&&restored&&reinstalled&&restoredAfterThrow&&
      suspended&&resumed&&fullscreen&&inlineAgain&&
      after&&ownerThread&&input==expected&&exits==1&&rendered>0&&
      committed>0&&committed<=rendered;
  std::string report=std::string(ok?"PASS ":"FAIL ")+
    "before="+std::to_string(before)+" during="+std::to_string(during)+
    " restored="+std::to_string(restored)+
    " reinstalled="+std::to_string(reinstalled)+
    " throw="+std::to_string(restoredAfterThrow)+
    " suspended="+std::to_string(suspended)+
    " resumed="+std::to_string(resumed)+
    " fullscreen="+std::to_string(fullscreen)+
    " inlineAgain="+std::to_string(inlineAgain)+
    " after="+std::to_string(after)+
    " owner="+std::to_string(ownerThread)+" inputSize="+
    std::to_string(input.size())+" events="+std::to_string(typedEvents)+
    " exits="+std::to_string(exits)+" rendered="+
    std::to_string(rendered)+" committed="+std::to_string(committed)+"\n";
  (void)!write(reportFd,report.data(),report.size());
  _exit(ok?0:1);
}
int main(){
  int report[2];
  if(pipe(report)!=0){std::cerr<<strerror(errno)<<'\n';return 2;}
  int master=-1;
  winsize size{};size.ws_row=24;size.ws_col=80;
  const pid_t pid=forkpty(&master,nullptr,nullptr,&size);
  if(pid<0){std::cerr<<strerror(errno)<<'\n';return 2;}
  if(pid==0){close(report[0]);child(report[1]);}
  close(report[1]);
  // Keep the PTY output drained while FTXUI initializes. Then inject a key
  // followed by a terminal DA response in the same ordered input write.
  bool sent=false;
  auto started=std::chrono::steady_clock::now();
  std::string diagnostic;
  for(;;){
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now()-started).count();
    if(!sent&&elapsed>=100){
      const char bytes[]="x\x1b[?1;2c";
      (void)!write(master,bytes,sizeof(bytes)-1);
      sent=true;
    }
    pollfd fds[2]={{master,POLLIN,0},{report[0],POLLIN,0}};
    const int ready=poll(fds,2,25);
    if(ready<0&&errno!=EINTR){std::cerr<<strerror(errno)<<'\n';break;}
    if(fds[0].revents&POLLIN){
      char bytes[65536];const auto count=read(master,bytes,sizeof(bytes));(void)count;
    }
    if(fds[1].revents&POLLIN){
      char bytes[512];auto n=read(report[0],bytes,sizeof(bytes));
      if(n>0)diagnostic.append(bytes,static_cast<std::size_t>(n));
    }
    if(diagnostic.find('\n')!=std::string::npos)break;
    if(elapsed>10000){::kill(pid,SIGKILL);break;}
  }
  int status=0;
  if(waitpid(pid,&status,0)<0){std::cerr<<strerror(errno)<<'\n';return 2;}
  if(diagnostic.empty()){
    char bytes[512];
    const auto n=read(report[0],bytes,sizeof(bytes));
    if(n>0)diagnostic.append(bytes,static_cast<std::size_t>(n));
  }
  close(master);close(report[0]);
  std::cout<<diagnostic;
  if(diagnostic.rfind("PASS ",0)!=0){
    std::cerr<<"No passing PTY report; child status="<<status<<'\n';
    return 2;
  }
  return WIFEXITED(status)?WEXITSTATUS(status):2;
}
