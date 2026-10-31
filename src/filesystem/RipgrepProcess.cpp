#include "filesystem/RipgrepProcess.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace batbox::filesystem {
namespace {
struct RawResult {
  std::string out,err;
  int exitCode=0;
  int execErrno=0;
  bool timedOut=false,aborted=false,overflow=false;
};
void throwErr(const char* operation){throw std::system_error(errno,std::generic_category(),operation);}
void makePipe(int (&fds)[2],bool closeOnExec=false){
  if(::pipe(fds)!=0)throwErr("pipe");
  if(closeOnExec){
    if(::fcntl(fds[1],F_SETFD,FD_CLOEXEC)<0)throwErr("fcntl");
  }
}
void closeFd(int& fd){if(fd>=0){::close(fd);fd=-1;}}
void nonblock(int fd){
  const int flags=::fcntl(fd,F_GETFL,0);
  if(flags<0||::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)throwErr("fcntl");
}
void killGroup(pid_t pid){
  if(pid>0){::kill(-pid,SIGKILL);::kill(pid,SIGKILL);}
}
void drain(int& fd,std::string& into,std::size_t limit,bool& overflow){
  if(fd<0)return;
  char chunk[8192];
  for(;;){
    const auto count=::read(fd,chunk,sizeof(chunk));
    if(count>0){
      const auto size=static_cast<std::size_t>(count);
      if(into.size()+size>limit){
        into.append(chunk,std::min(size,limit-into.size()));overflow=true;
      }else into.append(chunk,size);
      continue;
    }
    if(count==0){closeFd(fd);return;}
    if(errno==EAGAIN||errno==EWOULDBLOCK)return;
    if(errno==EINTR)continue;
    closeFd(fd);return;
  }
}
RawResult runRaw(const std::vector<std::string>& args,const std::string& target,
                 std::stop_token stopToken,const RipgrepConfig& config,bool singleThread){
  int outPipe[2]={-1,-1},errPipe[2]={-1,-1},execPipe[2]={-1,-1};
  makePipe(outPipe);makePipe(errPipe);makePipe(execPipe,true);
  std::vector<std::string> words={config.command};
  words.insert(words.end(),config.prefixArgs.begin(),config.prefixArgs.end());
  if(singleThread){words.push_back("-j");words.push_back("1");}
  words.insert(words.end(),args.begin(),args.end());
  words.push_back(target);
  std::vector<char*> argv;argv.reserve(words.size()+1);
  for(auto& word:words)argv.push_back(word.data());
  argv.push_back(nullptr);
  const pid_t pid=::fork();
  if(pid<0){closeFd(outPipe[0]);closeFd(outPipe[1]);closeFd(errPipe[0]);closeFd(errPipe[1]);
    closeFd(execPipe[0]);closeFd(execPipe[1]);throwErr("fork");}
  if(pid==0){
    ::setpgid(0,0);
    ::dup2(outPipe[1],STDOUT_FILENO);::dup2(errPipe[1],STDERR_FILENO);
    ::close(outPipe[0]);::close(outPipe[1]);::close(errPipe[0]);::close(errPipe[1]);::close(execPipe[0]);
    ::execvp(config.command.c_str(),argv.data());
    const int error=errno;
    (void)::write(execPipe[1],&error,sizeof(error));
    ::_exit(127);
  }
  closeFd(outPipe[1]);closeFd(errPipe[1]);closeFd(execPipe[1]);
  nonblock(outPipe[0]);nonblock(errPipe[0]);nonblock(execPipe[0]);
  RawResult result;
  std::string execBytes;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(config.timeoutMs);
  bool childExited=false,killed=false;
  int status=0;
  while(!childExited||outPipe[0]>=0||errPipe[0]>=0||execPipe[0]>=0){
    if(!killed&&stopToken.stop_requested()){result.aborted=true;killGroup(pid);killed=true;}
    if(!killed&&std::chrono::steady_clock::now()>=deadline){result.timedOut=true;killGroup(pid);killed=true;}
    struct pollfd fds[3]={{outPipe[0],POLLIN,0},{errPipe[0],POLLIN,0},{execPipe[0],POLLIN,0}};
    const int count=::poll(fds,3,50);
    if(count<0&&errno!=EINTR){killGroup(pid);break;}
    drain(outPipe[0],result.out,config.maxBufferBytes,result.overflow);
    drain(errPipe[0],result.err,config.maxBufferBytes,result.overflow);
    bool unused=false;
    drain(execPipe[0],execBytes,sizeof(int),unused);
    if(execBytes.size()==sizeof(int))std::memcpy(&result.execErrno,execBytes.data(),sizeof(int));
    if(result.overflow&&!killed){killGroup(pid);killed=true;}
    if(!childExited){const auto waited=::waitpid(pid,&status,WNOHANG);if(waited==pid)childExited=true;}
  }
  closeFd(outPipe[0]);closeFd(errPipe[0]);closeFd(execPipe[0]);
  if(!childExited)::waitpid(pid,&status,0);
  result.exitCode=WIFEXITED(status)?WEXITSTATUS(status):-1;
  return result;
}
std::vector<std::string> parseLines(const std::string& output,bool dropLast){
  const auto trimmed=value::trim(output);
  std::vector<std::string> lines;
  std::size_t start=0;
  while(start<=trimmed.size()){
    const auto end=trimmed.find('\n',start);
    std::string line=trimmed.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(!line.empty())lines.push_back(std::move(line));
    if(end==std::string::npos)break;
    start=end+1;
  }
  if(dropLast&&!lines.empty())lines.pop_back();
  return lines;
}
bool eagain(const std::string& error){return error.find("os error 11")!=std::string::npos||
  error.find("Resource temporarily unavailable")!=std::string::npos;}
} // namespace

std::vector<std::string> ripGrep(const std::vector<std::string>& args,
  const std::string& target,std::stop_token stopToken,const RipgrepConfig& config){
  auto raw=runRaw(args,target,stopToken,config,false);
  if(raw.exitCode==0&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return parseLines(raw.out,false);
  if(raw.exitCode==1&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return {};
  if(raw.execErrno==EAGAIN||eagain(raw.err))raw=runRaw(args,target,stopToken,config,true);
  if(raw.execErrno){
    if(raw.execErrno==ENOENT||raw.execErrno==EACCES||raw.execErrno==EPERM)
      throw std::system_error(raw.execErrno,std::generic_category(),"ripgrep");
  }
  if(raw.exitCode==0&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return parseLines(raw.out,false);
  if(raw.exitCode==1&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return {};
  const bool timeout=raw.timedOut||raw.aborted;
  auto lines=parseLines(raw.out,timeout||raw.overflow);
  if(timeout&&lines.empty())
    throw RipgrepTimeoutError("Ripgrep search timed out after "+
      std::to_string(config.timeoutDisplaySeconds)+
      " seconds. The search may have matched files but did not complete in time. Try searching a more specific path or pattern.",
      lines);
  return lines;
}
} // namespace batbox::filesystem
