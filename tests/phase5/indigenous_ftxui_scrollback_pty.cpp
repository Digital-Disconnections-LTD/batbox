#include "terminal_ftxui/Driver.hpp"
#include <ftxui/dom/elements.hpp>
#include <chrono>
#include <cstring>
#include <iostream>
#include <poll.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include <sys/wait.h>
#include <unistd.h>

using batbox::event_loop::Loop;
using batbox::terminal_ftxui::Callbacks;
using batbox::terminal_ftxui::CommittedRow;
using batbox::terminal_ftxui::Driver;
using batbox::terminal_ftxui::Options;

static void child(int reportFd){
  bool ownerInput=false;
  int renders=0;
  try{
    Loop owner;
    std::shared_ptr<Driver> driver;
    driver=std::make_shared<Driver>(owner,Options{},Callbacks{
      .render=[&]{++renders;return ftxui::text("live prompt");},
      .onRawInputBytes=[&](std::string_view bytes){
        ownerInput=owner.isOwnerThread()&&bytes=="x";
        driver->commitScrollback({CommittedRow{"c","history C"},
                                  CommittedRow{"b","history B"}});
      },
      .onEvent={},.onResize=[](ftxui::Dimensions){},
      .renderPlain={},.writePlain={},.onExit={}
    });
    driver->commitScrollback({CommittedRow{"a","history A"},
                              CommittedRow{"b","history B"}});
    driver->start();
    auto done=owner.setTimeout(400,[&]{driver->stop();});
    owner.run();
    done.cancel();
  }catch(const std::exception& error){
    const std::string report=std::string("FAIL ")+error.what()+"\n";
    (void)!write(reportFd,report.data(),report.size());
    _exit(2);
  }
  const auto report=std::string(ownerInput&&renders>=2?"PASS":"FAIL")+
    " owner="+std::to_string(ownerInput)+
    " renders="+std::to_string(renders)+"\n";
  (void)!write(reportFd,report.data(),report.size());
  _exit(ownerInput&&renders>=2?0:1);
}
static std::size_t countOf(const std::string& text,const std::string& needle){
  std::size_t count=0,offset=0;
  while((offset=text.find(needle,offset))!=std::string::npos){
    ++count;offset+=needle.size();
  }
  return count;
}
int main(){
  int report[2];if(pipe(report)!=0)return 2;
  int master=-1;
  const auto pid=forkpty(&master,nullptr,nullptr,nullptr);
  if(pid<0)return 2;
  if(pid==0){close(report[0]);child(report[1]);}
  close(report[1]);
  std::string output,diagnostic;
  bool sent=false;
  const auto start=std::chrono::steady_clock::now();
  for(;;){
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now()-start).count();
    if(!sent&&elapsed>=100){
      const char key='x';(void)!write(master,&key,1);sent=true;
    }
    pollfd fds[2]={{master,POLLIN,0},{report[0],POLLIN,0}};
    const auto ready=poll(fds,2,25);
    if(ready<0)break;
    if(fds[0].revents&POLLIN){
      char bytes[4096];const auto n=read(master,bytes,sizeof(bytes));
      if(n>0)output.append(bytes,static_cast<std::size_t>(n));
    }
    if(fds[1].revents&POLLIN){
      char bytes[512];const auto n=read(report[0],bytes,sizeof(bytes));
      if(n>0)diagnostic.append(bytes,static_cast<std::size_t>(n));
    }
    if(diagnostic.find('\n')!=std::string::npos||elapsed>2000)break;
  }
  int status=0;(void)waitpid(pid,&status,0);
  close(master);close(report[0]);
  const auto a=output.find("history A");
  const auto b=output.find("history B");
  const auto c=output.find("history C");
  const bool ordered=a!=std::string::npos&&b!=std::string::npos&&
    c!=std::string::npos&&a<b&&b<c;
  const bool unique=countOf(output,"history A")==1&&
    countOf(output,"history B")==1&&countOf(output,"history C")==1;
  const bool redrawn=output.find("live prompt",c)!=std::string::npos;
  const bool passed=diagnostic.rfind("PASS",0)==0&&ordered&&unique&&
    redrawn&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
  std::cout<<(passed?"PASS ":"FAIL ")<<"ordered="<<ordered
    <<" unique="<<unique<<" redrawn="<<redrawn<<' '
    <<diagnostic;
  return passed?0:1;
}
