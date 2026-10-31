#include "shell/ManagedShellInternal.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace batbox::shell {
Json ExecResult::toJson()const{
  Json result={{"stdout",stdoutText},{"stderr",stderrText},{"code",code},{"interrupted",interrupted}};
  if(backgroundMissionId)result["backgroundTaskId"]=*backgroundMissionId;
  if(backgroundedByUser)result["backgroundedByUser"]=true;
  if(assistantAutoBackgrounded||assistantAutoBackgroundedOwnProperty)
    result["assistantAutoBackgrounded"]=assistantAutoBackgrounded;
  if(outputFilePath)result["outputFilePath"]=*outputFilePath;
  if(outputFileSize)result["outputFileSize"]=*outputFileSize;
  if(outputMissionId)result["outputTaskId"]=*outputMissionId;
  if(preSpawnError)result["preSpawnError"]=*preSpawnError;
  return result;
}
Json Progress::toJson()const{
  Json result={{"type","progress"},{"output",lastLines},{"fullOutput",fullOutput},
    {"elapsedTimeSeconds",elapsedTimeSeconds},{"totalLines",totalLines},
    {"totalBytes",totalBytes},{"taskId",missionId}};
  if(timeoutMs)result["timeoutMs"]=*timeoutMs;
  return result;
}
std::string quotePosix(const std::string& value){
  std::string quoted="'";
  for(char c:value){if(c=='\'')quoted+="'\"'\"'";else quoted+=c;}
  return quoted+'\'';
}
std::string utf8WithReplacement(std::string_view bytes){
  std::string out;
  for(std::size_t i=0;i<bytes.size();){
    const auto c=static_cast<unsigned char>(bytes[i]);
    int count=c<0x80?1:(c>=0xc2&&c<=0xdf?2:(c>=0xe0&&c<=0xef?3:(c>=0xf0&&c<=0xf4?4:0)));
    bool valid=count!=0&&i+count<=bytes.size();
    if(valid&&count>1){
      for(int j=1;j<count;j++)if((static_cast<unsigned char>(bytes[i+j])&0xc0)!=0x80)valid=false;
      if(valid){const auto c1=static_cast<unsigned char>(bytes[i+1]);
        if((c==0xe0&&c1<0xa0)||(c==0xed&&c1>=0xa0)||(c==0xf0&&c1<0x90)||(c==0xf4&&c1>=0x90))valid=false;
      }
    }
    if(valid){out.append(bytes.substr(i,count));i+=count;}
    else{out+="\xef\xbf\xbd";++i;}
  }
  return out;
}
std::string readOutputPrefix(const std::string& path,std::size_t maxBytes,std::size_t& totalBytes){
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(path);
  std::error_code error;
  if(std::filesystem::is_symlink(indigenous,error)||error)
    throw std::runtime_error("output file could not be read (symlink)");
  const auto size=std::filesystem::file_size(indigenous,error);
  if(error)throw std::runtime_error("output file could not be read ("+error.message()+")");
  totalBytes=static_cast<std::size_t>(size);
  std::ifstream stream(indigenous,std::ios::binary);
  if(!stream)throw std::runtime_error("output file could not be read");
  std::string bytes(std::min(maxBytes,totalBytes),'\0');
  stream.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
  bytes.resize(static_cast<std::size_t>(stream.gcount()));
  return utf8WithReplacement(bytes);
#else
  struct stat info{};
  if(::stat(path.c_str(),&info)!=0)throw std::runtime_error("output file could not be read ("+std::to_string(errno)+")");
  totalBytes=static_cast<std::size_t>(info.st_size);
  const int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
  if(fd<0)throw std::runtime_error("output file could not be read ("+std::to_string(errno)+")");
  std::string bytes(std::min(maxBytes,totalBytes),'\0');
  std::size_t got=0;
  while(got<bytes.size()){
    const auto n=::pread(fd,bytes.data()+got,bytes.size()-got,got);
    if(n<0&&errno==EINTR)continue;
    if(n<=0)break;
    got+=static_cast<std::size_t>(n);
  }
  ::close(fd);bytes.resize(got);
  return utf8WithReplacement(bytes);
#endif
}
} // namespace batbox::shell
