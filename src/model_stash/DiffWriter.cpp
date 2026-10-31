#include "model_stash/DiffWriter.hpp"
#include "runtime/IndigenousPath.hpp"
#ifdef _WIN32
#include "process/IndigenousJsonlProcess.hpp"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <mutex>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace batbox::model_stash {
namespace {
std::string suffix(const DiffWriterServices& services){
  constexpr std::string_view chars="abcdefghijklmnopqrstuvwxyz0123456789";
  std::string result;
  for(int i=0;i<4;++i){
    const auto sample=services.random();
    const auto index=std::clamp(static_cast<int>(sample*chars.size()),0,
      static_cast<int>(chars.size())-1);
    result+=chars[index];
  }
  return result;
}
class TemporaryFile {
public:
  explicit TemporaryFile(const std::filesystem::path& directory) {
#ifdef _WIN32
    wchar_t name[MAX_PATH]{};
    if(!::GetTempFileNameW(directory.c_str(),L"bci",0,name))
      throw std::runtime_error("Cannot create cache diff input");
    path_=runtime::pathToUtf8(std::filesystem::path(name));
    descriptor_=::_wopen(name,O_WRONLY|_O_BINARY,0600);
    if(descriptor_<0){std::error_code ignored;std::filesystem::remove(name,ignored);
      throw std::runtime_error("Cannot open cache diff input");}
#else
    auto pattern=(directory/"cache-diff-input-XXXXXX").string();
    std::vector<char> writable(pattern.begin(),pattern.end());
    writable.push_back('\0');
    descriptor_=mkstemp(writable.data());
    if(descriptor_<0)throw std::runtime_error("Cannot create cache diff input");
    path_=writable.data();
#endif
  }
  ~TemporaryFile(){
    if(descriptor_>=0){
#ifdef _WIN32
      ::_close(descriptor_);
#else
      ::close(descriptor_);
#endif
    }
    if(!path_.empty()){
      std::error_code ignored;
      std::filesystem::remove(runtime::pathFromUtf8(path_),ignored);
    }
  }
  void writeContent(const std::string& content){
    std::size_t offset=0;
    while(offset<content.size()){
#ifdef _WIN32
      auto count=::_write(descriptor_,content.data()+offset,
        static_cast<unsigned int>(std::min<std::size_t>(content.size()-offset,0x7fffffff)));
#else
      auto count=::write(descriptor_,content.data()+offset,content.size()-offset);
#endif
      if(count<=0)throw std::runtime_error("Cannot write cache diff input");
      offset+=static_cast<std::size_t>(count);
    }
  }
  const std::string& path() const{return path_;}
private:
  int descriptor_=-1;
  std::string path_;
};
std::string createPatch(const std::string& before,const std::string& after,
    const std::filesystem::path& directory){
  TemporaryFile first(directory),second(directory);
  first.writeContent(before);second.writeContent(after);
#ifdef _WIN32
  std::mutex mutex;
  std::string output;
  process::IndigenousChildOptions options;
  options.lineMode=false;
  options.onData=[&](const std::string& chunk,bool stdoutPipe){
    if(!stdoutPipe)return;
    std::lock_guard lock(mutex);output+=chunk;
  };
  process::IndigenousJsonlProcess child({"diff","-U4","--label",
    "prompt-state\tbefore","--label","prompt-state\tafter",
    first.path(),second.path()},{},options);
  child.closeStdin();
  const auto code=child.waitForExit(std::chrono::steady_clock::now()+
    std::chrono::minutes(1));
  child.close();
  if(code>1)throw std::runtime_error("Cache diff failed");
  std::lock_guard lock(mutex);
  return "Index: prompt-state\n"+std::string(67,'=')+"\n"+output;
#else
  int descriptors[2];
  if(pipe(descriptors)!=0)throw std::runtime_error("Cannot open cache diff pipe");
  const auto pid=fork();
  if(pid<0){close(descriptors[0]);close(descriptors[1]);
    throw std::runtime_error("Cannot start cache diff");}
  if(pid==0){
    dup2(descriptors[1],STDOUT_FILENO);
    close(descriptors[0]);close(descriptors[1]);
    execlp("diff","diff","-U4","--label","prompt-state\tbefore",
      "--label","prompt-state\tafter",first.path().c_str(),
      second.path().c_str(),static_cast<char*>(nullptr));
    _exit(127);
  }
  close(descriptors[1]);
  std::string output;
  std::array<char,4096> buffer{};
  while(true){
    const auto count=read(descriptors[0],buffer.data(),buffer.size());
    if(count==0)break;
    if(count<0){close(descriptors[0]);waitpid(pid,nullptr,0);
      throw std::runtime_error("Cannot read cache diff output");}
    output.append(buffer.data(),static_cast<std::size_t>(count));
  }
  close(descriptors[0]);
  int status=0;
  if(waitpid(pid,&status,0)<0||!WIFEXITED(status)||WEXITSTATUS(status)>1)
    throw std::runtime_error("Cache diff failed");
  return "Index: prompt-state\n"+std::string(67,'=')+"\n"+output;
#endif
}
}

std::optional<std::string> writeStashBreakDiff(const std::string& before,
    const std::string& after,const DiffWriterServices& services){
  try{
    if(!services.tempDir||!services.random)
      throw std::invalid_argument("Cache diff imports are required");
    const auto directory=runtime::pathFromUtf8(services.tempDir());
    const auto path=directory/("cache-break-"+suffix(services)+".diff");
    std::filesystem::create_directories(directory);
    const auto patch=createPatch(before,after,directory);
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    if(!output)throw std::runtime_error("Cannot write cache break diff");
    output<<patch;
    if(!output)throw std::runtime_error("Cannot write cache break diff");
    return runtime::pathToUtf8(path);
  }catch(...){return std::nullopt;}
}
} // namespace batbox::model_stash
