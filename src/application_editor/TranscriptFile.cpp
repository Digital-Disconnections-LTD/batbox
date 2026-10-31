#include "application_editor/TranscriptFile.hpp"
#include "runtime/Platform.hpp"
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace batbox::application_editor {
std::shared_ptr<TranscriptFile> TranscriptFile::create(){
  const auto path=std::filesystem::temp_directory_path()/
    ("batbox-transcript-"+std::to_string(platform::nowMillis())+"-"+
      platform::randomUUID()+".txt");
#ifdef _WIN32
  const int fd=_wopen(path.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,
    _S_IREAD|_S_IWRITE);
#else
  const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
#endif
  if(fd<0)throw std::runtime_error("Could not create transcript temp file");
  return std::shared_ptr<TranscriptFile>(new TranscriptFile(path,fd));
}
TranscriptFile::~TranscriptFile(){
  try{finish();}catch(...){}
}
void TranscriptFile::append(const std::string& text){
  if(fd_<0)throw std::logic_error("Transcript temp file is closed");
  std::size_t offset=0;
  while(offset<text.size()){
#ifdef _WIN32
    const auto written=_write(fd_,text.data()+offset,
      static_cast<unsigned>(std::min<std::size_t>(text.size()-offset,1<<20)));
#else
    const auto written=::write(fd_,text.data()+offset,text.size()-offset);
    if(written<0&&errno==EINTR)continue;
#endif
    if(written<=0)throw std::runtime_error("Could not write transcript temp file");
    offset+=static_cast<std::size_t>(written);
  }
}
void TranscriptFile::finish(){
  if(fd_<0)return;
  const int fd=fd_;fd_=-1;
#ifdef _WIN32
  const int synced=_commit(fd);_close(fd);
#else
  const int synced=::fsync(fd);::close(fd);
#endif
  if(synced!=0)throw std::runtime_error("Could not sync transcript temp file");
}
}
