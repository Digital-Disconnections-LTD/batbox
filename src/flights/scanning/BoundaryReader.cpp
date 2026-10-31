#include "flights/scanning/Scanning.hpp"
#include "flights/IndigenousPath.hpp"
#include "flights/scanning/BoundaryCore.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <vector>

namespace batbox::flights::scanning {
namespace {
constexpr std::size_t transcriptReadChunkSize=1024*1024;
constexpr std::string_view boundaryMarker="\"compact_boundary\"";
class ReadDescriptor {
 public:
#if defined(_WIN32)
  explicit ReadDescriptor(const std::string& path):input_(indigenousPath(path),
      std::ios::binary){
    if(!input_)throw std::system_error(errno,std::generic_category(),"open transcript");
  }
  std::size_t read(char* buffer,std::size_t length,std::size_t offset){
    input_.clear();input_.seekg(static_cast<std::streamoff>(offset),std::ios::beg);
    if(!input_)throw std::runtime_error("seek transcript failed");
    input_.read(buffer,static_cast<std::streamsize>(length));
    if(input_.bad())throw std::runtime_error("read transcript failed");
    return static_cast<std::size_t>(input_.gcount());
  }
 private:
  std::ifstream input_;
#else
  explicit ReadDescriptor(const std::string& path):fd_(::open(path.c_str(),O_RDONLY)){
    if(fd_<0)throw std::system_error(errno,std::generic_category(),"open transcript");
  }
  ~ReadDescriptor(){if(fd_>=0)::close(fd_);}
  ReadDescriptor(const ReadDescriptor&)=delete;
  ReadDescriptor& operator=(const ReadDescriptor&)=delete;
  std::size_t read(char* buffer,std::size_t length,std::size_t offset){
    while(true){
      const auto n=::pread(fd_,buffer,length,static_cast<off_t>(offset));
      if(n>=0)return static_cast<std::size_t>(n);
      if(errno!=EINTR)throw std::system_error(errno,std::generic_category(),"read transcript");
    }
  }
 private:
  int fd_=-1;
#endif
};
}
ScanResult readTranscriptForLoad(const std::string& filePath,std::size_t fileSize){
  detail::LoadState state;
  state.out.reserve(std::min<std::size_t>(fileSize,8*1024*1024));
  std::vector<char> storage(transcriptReadChunkSize);
  ReadDescriptor file(filePath);
  std::size_t filePos=0;
  while(filePos<fileSize){
    const auto bytesRead=file.read(storage.data(),
      std::min(transcriptReadChunkSize,fileSize-filePos),filePos);
    if(bytesRead==0)break;
    filePos+=bytesRead;
    const std::string_view chunk(storage.data(),bytesRead);
    const auto chunkOff=detail::processStraddle(state,chunk);
    std::string combined;
    std::string_view buffer;
    if(!state.carry.empty()){
      combined.reserve(state.carry.size()+bytesRead-chunkOff);
      combined+=state.carry;
      combined+=chunk.substr(chunkOff);
      buffer=combined;
    }else buffer=chunk.substr(chunkOff);
    const auto scan=detail::scanChunkLines(state,buffer,boundaryMarker);
    detail::captureSnap(state,buffer,chunk,scan);
    detail::captureCarry(state,buffer,scan.trailStart);
    state.bufFileOff+=scan.trailStart;
  }
  detail::finalizeOutput(state);
  return {state.boundaryStartOffset,std::move(state.out),state.hasPreservedSegment};
}
void bindIndigenousLoadScanners(LoadServices& services){
  services.readTranscriptForLoad=readTranscriptForLoad;
  services.scanPreBoundaryMetadata=scanPreBoundaryMetadata;
  services.walkChainBeforeParse=walkChainBeforeParse;
}
} // namespace batbox::flights::scanning
