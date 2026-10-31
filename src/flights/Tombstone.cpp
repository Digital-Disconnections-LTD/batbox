#include "flights/Tombstone.hpp"
#include "flights/IndigenousPath.hpp"
#include "runtime/JsonValue.hpp"
#include "formatting/Format.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace batbox::flights {
namespace {
#if !defined(_WIN32)
struct File {
  int fd=-1;
  explicit File(const std::string& path):fd(::open(path.c_str(),O_RDWR|O_CLOEXEC)) {
    if(fd<0)throw std::system_error(errno,std::generic_category(),"open");
  }
  void close(){const auto value=fd;fd=-1;if(::close(value)<0)throw std::system_error(errno,std::generic_category(),"close");}
  ~File(){if(fd>=0)(void)::close(fd);}
};
#endif
}
void removeMessageFromFile(const std::string& path,const std::string& uuid,const TombstoneDiagnostic& diagnostic) {
  try {
#if defined(_WIN32)
    const auto indigenous=indigenousPath(path);
    const auto size=std::filesystem::file_size(indigenous);
    if(size==0)return;
    const auto length=static_cast<std::size_t>(std::min<std::uintmax_t>(size,65536));
    const auto tailStart=size-length;
    std::ifstream tailInput(indigenous,std::ios::binary);
    if(!tailInput)throw std::runtime_error("readFile");
    tailInput.seekg(static_cast<std::streamoff>(tailStart));
    std::string tail(length,'\0');tailInput.read(tail.data(),
      static_cast<std::streamsize>(length));
    tail.resize(static_cast<std::size_t>(tailInput.gcount()));
    tailInput.close();
    const auto needle="\"uuid\":\""+uuid+'"';
    const auto match=tail.rfind(needle);
    if(match!=std::string::npos){
      const auto previous=tail.rfind('\n',match);
      if(previous!=std::string::npos||tailStart==0){
        const auto lineStart=previous==std::string::npos?0:previous+1;
        const auto next=tail.find('\n',match+needle.size());
        const auto end=next==std::string::npos?tail.size():next+1;
        const auto absolute=tailStart+lineStart;
        if(end<tail.size()){
          std::fstream output(indigenous,std::ios::binary|std::ios::in|std::ios::out);
          if(!output)throw std::runtime_error("writeFile");
          output.seekp(static_cast<std::streamoff>(absolute));
          output.write(tail.data()+end,static_cast<std::streamsize>(tail.size()-end));
          output.close();if(!output)throw std::runtime_error("writeFile");
        }
        std::filesystem::resize_file(indigenous,absolute+tail.size()-end);
        return;
      }
    }
    if(size>50*1024*1024){
      if(diagnostic)diagnostic("Skipping tombstone removal: session file too large ("+
        formatting::formatFileSize(static_cast<double>(size))+")","warn");
      return;
    }
#else
    off_t size=0;
    {
      File file(path);struct stat info{};
      if(::fstat(file.fd,&info)<0)throw std::system_error(errno,std::generic_category(),"fstat");
      size=info.st_size;if(size==0){file.close();return;}
      const auto length=static_cast<std::size_t>(std::min(size,off_t(65536)));const auto start=size-static_cast<off_t>(length);
      std::string tail(length,'\0');ssize_t count;
      do{count=::pread(file.fd,tail.data(),length,start);}while(count<0 && errno==EINTR);
      if(count<0)throw std::system_error(errno,std::generic_category(),"pread");
      tail.resize(count);
      const auto needle="\"uuid\":\""+uuid+'"';const auto match=tail.rfind(needle);
      if(match!=std::string::npos) {
        const auto previous=tail.rfind('\n',match);
        if(previous!=std::string::npos || start==0) {
          const auto lineStart=previous==std::string::npos ? 0 : previous+1;
          const auto next=tail.find('\n',match+needle.size());const auto end=next==std::string::npos?tail.size():next+1;
          const auto absolute=start+static_cast<off_t>(lineStart);
          if(::ftruncate(file.fd,absolute)<0)throw std::system_error(errno,std::generic_category(),"ftruncate");
          if(end<tail.size()) {
            ssize_t written;do{written=::pwrite(file.fd,tail.data()+end,tail.size()-end,absolute);}while(written<0 && errno==EINTR);
            if(written<0)throw std::system_error(errno,std::generic_category(),"pwrite");
          }
          file.close();return;
        }
      }
      file.close();
    }
    if(size>50*1024*1024) {
      if(diagnostic)diagnostic("Skipping tombstone removal: session file too large ("+formatting::formatFileSize(static_cast<double>(size))+")","warn");
      return;
    }
#endif
    std::ifstream input(indigenousPath(path),std::ios::binary);
    if(!input)throw std::system_error(errno,std::generic_category(),"readFile");
    const std::string bytes((std::istreambuf_iterator<char>(input)),{});if(input.bad())throw std::runtime_error("readFile");
    const auto content=value::decodeUtf8(bytes);std::string result;bool first=true;std::size_t start=0;
    while(start<=content.size()) {
      const auto next=content.find('\n',start);const auto end=next==std::string::npos?content.size():next;
      const auto line=content.substr(start,end-start);bool keep=true;
      if(!value::trim(line).empty())try{keep=value::property(value::parse(line),"uuid")!=uuid;}catch(const std::exception&){}
      if(keep){if(!first)result+='\n';result+=line;first=false;}
      if(next==std::string::npos)break;
      start=next+1;
    }
    input.close();std::ofstream output(indigenousPath(path),
      std::ios::binary|std::ios::trunc);if(!output)throw std::runtime_error("writeFile");
    output<<result;output.close();if(!output)throw std::runtime_error("writeFile");
  }catch(const std::exception&){} // Archived tombstone errors are deliberately ignored.
}
}
