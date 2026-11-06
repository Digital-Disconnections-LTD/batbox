#include "shell/ManagedShell.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
using namespace batbox::shell;
namespace {
void check(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
// Run as the Bash command's child: name every descriptor above stderr that
// this process holds.
int listDescriptors(){
  const long limit=std::min<long>(::sysconf(_SC_OPEN_MAX),65536);
  for(int fd=3;fd<limit;++fd)
    if(::fcntl(fd,F_GETFD)!=-1)std::cout<<"open "<<fd<<'\n';
  return 0;
}
std::string quoted(const std::string& value){
  std::string out="'";
  for(char c:value){if(c=='\'')out+="'\\''";else out+=c;}
  return out+"'";
}
bool inheritable(int fd){
  const int flags=::fcntl(fd,F_GETFD);
  return flags!=-1&&!(flags&FD_CLOEXEC);
}
struct RemoveOnExit {
  std::filesystem::path path;
  ~RemoveOnExit(){std::error_code error;std::filesystem::remove_all(path,error);}
};
}
int main(int argc,char** argv){
  if(argc==2&&std::string(argv[1])=="--list-descriptors")return listDescriptors();
  try{
    char dir[]="/tmp/batbox-descriptors-XXXXXX";
    check(::mkdtemp(dir)!=nullptr,"mkdtemp failed");
    const std::filesystem::path root=std::filesystem::canonical(dir);
    const RemoveOnExit cleanup{root};
    const auto self=std::filesystem::absolute(argv[0]).string();
    // What Batbox keeps open while it runs: an append-mode log stream, a
    // descriptor opened without O_CLOEXEC and a pipe.
    std::ofstream log(root/"native.log",std::ios::app);
    check(log.is_open(),"log stream did not open");
    log<<"log line\n";log.flush();
    const int plain=::open((root/"transcript.jsonl").c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
    check(plain>=0,std::string("open transcript failed: ")+std::strerror(errno));
    int pipeFds[2];
    check(::pipe(pipeFds)==0,"pipe failed");
    check(inheritable(plain)&&inheritable(pipeFds[0])&&inheritable(pipeFds[1]),
      "the planted descriptors are not inheritable, so the check proves nothing");
    std::atomic<int> next{0};
    FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
    config.outputDir=(root/"jobs").string();
    config.generateMissionId=[&]{return "fd"+std::to_string(++next);};
    config.splitCommand=[](const std::string& command){return std::vector<std::string>{command};};
    ManagedShell shell(config);
    const auto command=quoted(self)+" --list-descriptors; echo probe-done";
    const auto foreground=shell.run({.command=command});
    check(foreground.code==0&&foreground.stdoutText.find("probe-done")!=std::string::npos,
      "foreground probe did not run: "+foreground.toJson().dump());
    check(foreground.stdoutText.find("open ")==std::string::npos,
      "a foreground Bash child inherited descriptors:\n"+foreground.stdoutText);
    const auto started=shell.run({.command=command,.runInBackground=true});
    check(started.backgroundMissionId.has_value(),"background probe has no mission ID");
    const auto background=shell.wait(*started.backgroundMissionId,10000);
    check(background.code==0&&background.stdoutText.find("probe-done")!=std::string::npos,
      "background probe did not run: "+background.toJson().dump());
    check(background.stdoutText.find("open ")==std::string::npos,
      "a background Bash child inherited descriptors:\n"+background.stdoutText);
    shell.cleanup(*started.backgroundMissionId);
    ::close(plain);::close(pipeFds[0]);::close(pipeFds[1]);log.close();
    std::cout<<"PASS Bash children inherit only stdin, stdout and stderr\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<"FAIL "<<error.what()<<'\n';return 1;
  }
}
