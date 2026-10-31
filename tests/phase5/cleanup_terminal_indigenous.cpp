#include "cleanup/Terminal.hpp"
#include <cassert>
#include <cerrno>
#include <csignal>
#include <pthread.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

void brokenPipe(bool ignored,bool blocked,bool pending){
  int pipeFds[2];assert(::pipe(pipeFds)==0);
  ::close(pipeFds[0]);
  const auto child=::fork();assert(child>=0);
  if(child==0){
    ::alarm(5);
    ::signal(SIGPIPE,ignored?SIG_IGN:SIG_DFL);
    sigset_t mask;sigemptyset(&mask);sigaddset(&mask,SIGPIPE);
    assert(pthread_sigmask(blocked?SIG_BLOCK:SIG_UNBLOCK,&mask,nullptr)==0);
    if(pending)assert(::raise(SIGPIPE)==0);
    if(::dup2(pipeFds[1],STDOUT_FILENO)<0)::_exit(3);
    ::close(pipeFds[1]);
    try{batbox::cleanup::indigenousWriteStdout("terminal reset");}
    catch(const std::system_error& error){
      sigset_t current;assert(pthread_sigmask(SIG_BLOCK,nullptr,&current)==0);
      assert((sigismember(&current,SIGPIPE)==1)==blocked);
      assert(sigpending(&current)==0);
      assert((sigismember(&current,SIGPIPE)==1)==(blocked&&!ignored));
      ::_exit(error.code()==std::errc::broken_pipe?0:4);
    }
    ::_exit(5);
  }
  ::close(pipeFds[1]);
  int status=0;assert(::waitpid(child,&status,0)==child);
  assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);
}
int main(){
  brokenPipe(false,false,false);
  brokenPipe(true,false,false);
  brokenPipe(false,true,false);
  brokenPipe(false,true,true);
}
