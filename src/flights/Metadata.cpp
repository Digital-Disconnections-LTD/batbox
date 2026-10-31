#include "flights/Metadata.hpp"
#include "flights/IndigenousPath.hpp"
#include "flights/WriteQueue.hpp"
#include "runtime/Paths.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace batbox::flights {
std::string unescapeJsonString(const std::string& raw) {
  if(raw.find('\\')==std::string::npos)return raw;
  try{return value::parse('"'+raw+'"').get<std::string>();}catch(const std::exception&){return raw;}
}
std::optional<std::string> extractJsonStringField(const std::string& text,const std::string& key) {
  for(const auto& pattern:{'"'+key+"\":\"",'"'+key+"\": \""}) {
    const auto at=text.find(pattern);if(at==std::string::npos)continue;
    const auto start=at+pattern.size();auto i=start;
    while(i<text.size()) {
      if(text[i]=='\\'){i+=2;continue;}
      if(text[i]=='"')return unescapeJsonString(text.substr(start,i-start));
      ++i;
    }
  }
  return std::nullopt;
}
std::optional<std::string> extractLastJsonStringField(const std::string& text,const std::string& key) {
  std::optional<std::string> last;
  for(const auto& pattern:{'"'+key+"\":\"",'"'+key+"\": \""}) {
    std::size_t from=0;
    while(true) {
      const auto at=text.find(pattern,from);if(at==std::string::npos)break;
      const auto start=at+pattern.size();auto i=start;
      while(i<text.size()) {
        if(text[i]=='\\'){i+=2;continue;}
        if(text[i]=='"'){last=unescapeJsonString(text.substr(start,i-start));break;}
        ++i;
      }
      from=i+1;
    }
  }
  return last;
}
std::string readFileTailSync(const std::string& file) {
#if defined(_WIN32)
  std::ifstream input(indigenousPath(file),std::ios::binary|std::ios::ate);
  if(!input)return "";
  const auto end=input.tellg();
  if(end<=0)return "";
  const auto count=static_cast<std::streamsize>(std::min<std::streamoff>(
    static_cast<std::streamoff>(end),65536));
  input.seekg(-count,std::ios::end);
  std::string buffer(static_cast<std::size_t>(count),'\0');
  input.read(buffer.data(),count);
  buffer.resize(static_cast<std::size_t>(input.gcount()));
  return value::decodeUtf8(buffer);
#else
  const int fd=::open(file.c_str(),O_RDONLY|O_CLOEXEC);if(fd<0)return "";
  struct stat info{};std::string result;
  if(::fstat(fd,&info)==0) {
    constexpr off_t limit=65536;const auto offset=std::max(off_t(0),info.st_size-limit);
    std::string buffer(static_cast<std::size_t>(std::min(limit,info.st_size-offset)),'\0');
    const auto count=::pread(fd,buffer.data(),buffer.size(),offset);
    if(count>=0)result=value::decodeUtf8(std::string_view(buffer).substr(0,count));
  }
  (void)::close(fd);return result;
#endif
}
void appendEntryToFile(const std::string& file,const Json& entry) {
  auto services=indigenousQueueServices();const auto line=value::stringify(entry)+'\n';
  try{services.appendFile(file,line);}
  catch(...) {
    const auto parent=value::dirname(file);
#if defined(_WIN32)
    std::filesystem::create_directories(indigenousPath(parent));
#else
    if(::mkdir(parent.c_str(),0700)<0)throw std::system_error(errno,std::generic_category(),"mkdirSync");
#endif
    services.appendFile(file,line);
  }
}
}
