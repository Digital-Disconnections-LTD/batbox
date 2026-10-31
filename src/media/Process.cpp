#include "media/Process.hpp"
#ifdef _WIN32
#include "process/IndigenousJsonlProcess.hpp"
#include <mutex>
#else
#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>

#ifndef _WIN32
extern char** environ;
#endif
namespace batbox::media::indigenous {
namespace {
#ifndef _WIN32
void closeFd(int& fd){if(fd>=0){::close(fd);fd=-1;}}
void drain(int& fd,std::string& text){
  if(fd<0)return;
  char buffer[8192];
  while(true){
    const auto count=::read(fd,buffer,sizeof(buffer));
    if(count>0){if(text.size()<1024*1024)text.append(buffer,
      std::min<std::size_t>(count,1024*1024-text.size()));continue;}
    if(count==0){closeFd(fd);return;}
    if(errno==EINTR)continue;
    if(errno==EAGAIN||errno==EWOULDBLOCK)return;
    closeFd(fd);return;
  }
}
#endif
}
ProcessResult runProcess(const std::string& program,const std::vector<std::string>& args,int timeoutMs){
#ifdef _WIN32
  std::vector<std::string> command{program};
  command.insert(command.end(),args.begin(),args.end());
  struct Capture {std::mutex mutex;ProcessResult result;};
  auto capture=std::make_shared<Capture>();
  process::IndigenousChildOptions options;
  options.lineMode=false;
  options.onData=[capture](const std::string& bytes,bool stdoutPipe){
    std::lock_guard lock(capture->mutex);
    auto& text=stdoutPipe?capture->result.stdoutText:capture->result.stderrText;
    if(text.size()<1024*1024)
      text.append(bytes,0,std::min<std::size_t>(bytes.size(),1024*1024-text.size()));
  };
  try{
    process::IndigenousJsonlProcess child(command,{},std::move(options));
    child.closeStdin();
    try{
      const int code=child.waitForExit(std::chrono::steady_clock::now()+
        std::chrono::milliseconds(timeoutMs));
      child.close();
      std::lock_guard lock(capture->mutex);
      capture->result.code=code;
      return capture->result;
    }catch(const std::exception& error){
      if(std::string(error.what())!="Child process wait timed out")throw;
      child.kill();
      try{(void)child.waitForExit(std::chrono::steady_clock::now()+
        std::chrono::seconds(5));}catch(...){}
      child.close();
      std::lock_guard lock(capture->mutex);
      capture->result.code=-1;
      capture->result.stderrText+="timed out";
      return capture->result;
    }
  }catch(const process::IndigenousChildSpawnError& error){
    return {127,"",error.what()};
  }
#else
  int out[2]{-1,-1},err[2]{-1,-1};
#if defined(__APPLE__)
  bool pipeReady=::pipe(out)==0&&::pipe(err)==0;
  if(pipeReady)for(int fd:{out[0],out[1],err[0],err[1]})
    if(::fcntl(fd,F_SETFD,FD_CLOEXEC)<0)pipeReady=false;
  if(!pipeReady){
#else
  if(::pipe2(out,O_CLOEXEC)!=0||::pipe2(err,O_CLOEXEC)!=0){
#endif
    closeFd(out[0]);closeFd(out[1]);closeFd(err[0]);closeFd(err[1]);
    throw std::runtime_error("Cannot create media process pipes");
  }
  for(int fd:{out[0],err[0]}){
    const int flags=::fcntl(fd,F_GETFL,0);
    if(flags<0||::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){
      closeFd(out[0]);closeFd(out[1]);closeFd(err[0]);closeFd(err[1]);
      throw std::runtime_error("Cannot set media pipe read mode");
    }
  }
  posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions,out[1],STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions,err[1],STDERR_FILENO);
  for(int fd:{out[0],out[1],err[0],err[1]})posix_spawn_file_actions_addclose(&actions,fd);
  posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);
  posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attributes,0);
  std::vector<char*> argv;argv.push_back(const_cast<char*>(program.c_str()));
  for(const auto& arg:args)argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);
  pid_t child=-1;
  const int spawnError=posix_spawnp(&child,argv[0],&actions,&attributes,argv.data(),environ);
  posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);
  closeFd(out[1]);closeFd(err[1]);
  if(spawnError){closeFd(out[0]);closeFd(err[0]);return {127,"",std::strerror(spawnError)};}
  ProcessResult result;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);
  bool reaped=false,timedOut=false;int status=0;
  while(out[0]>=0||err[0]>=0||!reaped){
    const auto now=std::chrono::steady_clock::now();
    if(now>=deadline){::kill(-child,SIGKILL);while(::waitpid(child,&status,0)<0&&errno==EINTR){}
      result.code=-1;result.stderrText+="timed out";reaped=true;timedOut=true;break;}
    pollfd fds[2];nfds_t count=0;
    if(out[0]>=0)fds[count++]={out[0],POLLIN|POLLHUP,0};
    if(err[0]>=0)fds[count++]={err[0],POLLIN|POLLHUP,0};
    const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-now).count();
    (void)::poll(fds,count,static_cast<int>(std::min<std::int64_t>(remaining,50)));
    drain(out[0],result.stdoutText);drain(err[0],result.stderrText);
    if(!reaped){const auto done=::waitpid(child,&status,WNOHANG);
      if(done==child)reaped=true;else if(done<0&&errno==ECHILD)reaped=true;}
  }
  closeFd(out[0]);closeFd(err[0]);
  if(!reaped)while(::waitpid(child,&status,0)<0&&errno==EINTR){}
  if(timedOut)return result;
  result.code=WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?128+WTERMSIG(status):-1;
  return result;
#endif
}
} // namespace batbox::media::indigenous
