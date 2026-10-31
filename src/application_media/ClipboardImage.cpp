#include "application_media/ClipboardImage.hpp"
#ifdef __APPLE__
#include "application_media/ClipboardImageMac.hpp"
#endif
#include "media/Base64.hpp"
#include "media/ImageIndigenous.hpp"
#include "media/Media.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace batbox::application_media::indigenous {
namespace {
constexpr std::size_t maxClipboardBytes=32*1024*1024;
#ifndef _WIN32
std::optional<std::string> readProcess(const std::string& file,
    const std::vector<std::string>& args,int timeoutMs=3000,bool allowEmpty=false){
  int pipeFd[2];if(::pipe(pipeFd)!=0)return {};
  const int nullFd=::open("/dev/null",O_WRONLY);
  if(nullFd<0){::close(pipeFd[0]);::close(pipeFd[1]);return {};}
  posix_spawn_file_actions_t actions;
  if(posix_spawn_file_actions_init(&actions)!=0){
    ::close(pipeFd[0]);::close(pipeFd[1]);::close(nullFd);return {};
  }
  posix_spawn_file_actions_adddup2(&actions,pipeFd[1],STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions,nullFd,STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions,pipeFd[0]);
  posix_spawn_file_actions_addclose(&actions,pipeFd[1]);
  posix_spawn_file_actions_addclose(&actions,nullFd);
  std::vector<std::string> values;values.push_back(file);
  values.insert(values.end(),args.begin(),args.end());
  std::vector<char*> argv;argv.reserve(values.size()+1);
  for(auto& value:values)argv.push_back(value.data());
  argv.push_back(nullptr);
  pid_t pid=-1;
  const int spawned=posix_spawnp(&pid,file.c_str(),&actions,nullptr,
    argv.data(),environ);
  posix_spawn_file_actions_destroy(&actions);
  ::close(pipeFd[1]);::close(nullFd);
  if(spawned!=0){::close(pipeFd[0]);return {};}
  std::string output;output.reserve(16384);
  const auto deadline=std::chrono::steady_clock::now()+
    std::chrono::milliseconds(timeoutMs);
  bool failed=false,eof=false;
  char buffer[8192];
  while(!eof&&std::chrono::steady_clock::now()<deadline){
    pollfd descriptor{pipeFd[0],POLLIN|POLLHUP,0};
    const int remaining=static_cast<int>(std::chrono::duration_cast<
      std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count());
    const int polled=::poll(&descriptor,1,std::max(1,std::min(remaining,100)));
    if(polled<0){if(errno==EINTR)continue;failed=true;break;}
    if(polled==0)continue;
    const auto count=::read(pipeFd[0],buffer,sizeof(buffer));
    if(count<0){if(errno==EINTR)continue;failed=true;break;}
    if(count==0){eof=true;break;}
    if(output.size()+static_cast<std::size_t>(count)>maxClipboardBytes){failed=true;break;}
    output.append(buffer,static_cast<std::size_t>(count));
  }
  ::close(pipeFd[0]);
  if(!eof||failed)::kill(pid,SIGKILL);
  int status=0;
  while(::waitpid(pid,&status,0)<0&&errno==EINTR){}
  if(!eof||failed||!WIFEXITED(status)||WEXITSTATUS(status)!=0||
      (!allowEmpty&&output.empty()))return {};
  return output;
}
#endif

std::optional<std::string> imageBytes(){
#ifdef __linux__
  // Try PNG from X11/Wayland, then BMP for Windows clipboard bridges.
  for(const auto& [file,args]:std::vector<std::pair<std::string,
      std::vector<std::string>>>{
      {"xclip",{"-selection","clipboard","-t","image/png","-o"}},
      {"wl-paste",{"--type","image/png"}},
      {"xclip",{"-selection","clipboard","-t","image/bmp","-o"}},
      {"wl-paste",{"--type","image/bmp"}}}){
    if(auto bytes=readProcess(file,args))return bytes;
  }
  return {};
#elif defined(__APPLE__)
  if(auto png=readMacClipboardPng())return png;
  std::string path="/tmp/batbox-image-paste-XXXXXX.png";
  std::vector<char> name(path.begin(),path.end());name.push_back('\0');
  const int fd=::mkstemps(name.data(),4);
  if(fd<0)return {};
  ::close(fd);path=name.data();
  const std::vector<std::string> args={
    "-e","on run argv",
    "-e","set png_data to (the clipboard as «class PNGf»)",
    "-e","set fp to open for access POSIX file (item 1 of argv) with write permission",
    "-e","write png_data to fp",
    "-e","close access fp",
    "-e","end run",path};
  const auto result=readProcess("osascript",args,3000,true);
  std::optional<std::string> bytes;
  if(result){
    std::ifstream input(path,std::ios::binary);
    if(input)bytes=std::string(std::istreambuf_iterator<char>(input),{});
  }
  std::error_code error;std::filesystem::remove(path,error);
  return bytes&& !bytes->empty()?bytes:std::nullopt;
#elif defined(_WIN32)
  // The script writes raw PNG bytes to stdout; unlike the normal command
  // adapter this keeps binary data out of UTF-8 transcoding.
  constexpr const char* command=
    "powershell -NoProfile -NonInteractive -Command \"$img=Get-Clipboard -Format Image; "
    "if ($img) {$s=New-Object System.IO.MemoryStream; "
    "$img.Save($s,[System.Drawing.Imaging.ImageFormat]::Png); "
    "$b=$s.ToArray(); [Console]::OpenStandardOutput().Write($b,0,$b.Length)}\"";
  FILE* pipe=_popen(command,"rb");if(!pipe)return {};
  std::string output;char buffer[8192];
  while(const auto count=std::fread(buffer,1,sizeof(buffer),pipe)){
    if(output.size()+count>maxClipboardBytes){output.clear();break;}
    output.append(buffer,count);
  }
  const int code=_pclose(pipe);
  if(code!=0||output.empty())return {};
  return output;
#else
  return {};
#endif
}
}
Json getImageFromClipboard(){
  try{
    auto bytes=imageBytes();if(!bytes)return Json();
    if(bytes->size()>=2&&(*bytes)[0]=='B'&&(*bytes)[1]=='M')
      *bytes=media::indigenous::encode(*bytes,0,0,"png",80,false,0);
    const auto detected=media::detectImageFormatFromBuffer(*bytes);
    auto resized=media::maybeResizeAndDownsampleImageBuffer(*bytes,bytes->size(),
      detected.starts_with("image/")?detected.substr(6):"png");
    const std::string base64=media::indigenous::base64Encode(resized.buffer);
    return Json{{"base64",base64},
      {"mediaType",media::detectImageFormatFromBuffer(resized.buffer)},
      {"dimensions",resized.dimensions}};
  }catch(...){return Json();}
}
std::optional<std::string> getImagePathFromClipboard(){
  std::optional<std::string> path;
#ifdef __linux__
  path=readProcess("xclip",{"-selection","clipboard","-t","text/plain","-o"},
    3000);
  if(!path)path=readProcess("wl-paste",{},3000);
#elif defined(__APPLE__)
  path=readMacClipboardFilePath();
  if(!path)path=readProcess("osascript",{"-e",
    "get POSIX path of (the clipboard as «class furl»)"},3000);
#elif defined(_WIN32)
  FILE* pipe=_popen("powershell -NoProfile -NonInteractive -Command \"Get-Clipboard\"",
    "rb");
  if(pipe){
    std::string output;char buffer[1024];
    while(const auto count=std::fread(buffer,1,sizeof(buffer),pipe)){
      if(output.size()+count>32*1024){output.clear();break;}
      output.append(buffer,count);
    }
    if(_pclose(pipe)==0&&!output.empty())path=std::move(output);
  }
#endif
  if(!path)return {};
  const auto first=path->find_first_not_of(" \t\r\n");
  if(first==std::string::npos)return {};
  const auto last=path->find_last_not_of(" \t\r\n");
  return path->substr(first,last-first+1);
}
} // namespace batbox::application_media::indigenous
