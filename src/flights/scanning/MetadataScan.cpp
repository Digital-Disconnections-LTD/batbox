#include "flights/scanning/Scanning.hpp"
#include "flights/IndigenousPath.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>
#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace batbox::flights::scanning {
namespace {
constexpr std::array<std::string_view,9> metadataMarkers={
  "\"type\":\"summary\"","\"type\":\"custom-title\"","\"type\":\"tag\"",
  "\"type\":\"agent-name\"","\"type\":\"agent-color\"","\"type\":\"agent-setting\"",
  "\"type\":\"mode\"","\"type\":\"worktree-state\"","\"type\":\"pr-link\""};
constexpr std::size_t metadataPrefixBound=25,chunkSize=64*1024;
std::optional<std::string> resolveMetadataBuf(const std::optional<std::string>& carry,
  std::string_view chunk){
  if(!carry||carry->empty())return std::string(chunk);
  if(carry->size()<metadataPrefixBound)return *carry+std::string(chunk);
  if((*carry)[0]=='{'){
    for(const auto marker:metadataMarkers)
      if(carry->size()>=marker.size()+1&&
         std::string_view(*carry).substr(1,marker.size())==marker)
        return *carry+std::string(chunk);
  }
  const auto firstNl=chunk.find('\n');
  if(firstNl==std::string_view::npos)return std::nullopt;
  return std::string(chunk.substr(firstNl+1));
}
}
std::vector<std::string> scanPreBoundaryMetadata(const std::string& filePath,
  std::size_t endOffset){
#if defined(_WIN32)
  std::ifstream input(indigenousPath(filePath),std::ios::binary);
  if(!input)throw std::system_error(errno,std::generic_category(),"open metadata scan");
#else
  const int fd=::open(filePath.c_str(),O_RDONLY);
  if(fd<0)throw std::system_error(errno,std::generic_category(),"open metadata scan");
#endif
  std::vector<std::string> metadataLines;
  std::optional<std::string> carry;
  std::array<char,chunkSize> chunk{};
  std::size_t filePos=0;
#if !defined(_WIN32)
  try{
#endif
    while(filePos<endOffset){
#if defined(_WIN32)
      input.read(chunk.data(),static_cast<std::streamsize>(
        std::min(chunkSize,endOffset-filePos)));
      if(input.bad())throw std::runtime_error("read metadata scan failed");
      const auto n=input.gcount();
#else
      ssize_t n;
      do{n=::pread(fd,chunk.data(),std::min(chunkSize,endOffset-filePos),
        static_cast<off_t>(filePos));}while(n<0&&errno==EINTR);
      if(n<0)throw std::system_error(errno,std::generic_category(),"read metadata scan");
#endif
      if(n==0)break;
      filePos+=static_cast<std::size_t>(n);
      const auto buffer=resolveMetadataBuf(carry,
        std::string_view(chunk.data(),static_cast<std::size_t>(n)));
      if(!buffer){carry.reset();continue;}
      bool hasAnyMarker=false;
      for(const auto marker:metadataMarkers)if(buffer->find(marker)!=std::string::npos){
        hasAnyMarker=true;break;}
      if(hasAnyMarker){
        std::size_t lineStart=0,nl=buffer->find('\n');
        while(nl!=std::string::npos){
          for(const auto marker:metadataMarkers){
            const auto at=buffer->find(marker,lineStart);
            if(at!=std::string::npos&&at<nl){
              metadataLines.push_back(value::decodeUtf8(std::string_view(*buffer).substr(
                lineStart,nl-lineStart)));
              break;
            }
          }
          lineStart=nl+1;nl=buffer->find('\n',lineStart);
        }
        carry=buffer->substr(lineStart);
      }else{
        const auto lastNl=buffer->rfind('\n');
        carry=lastNl==std::string::npos?*buffer:buffer->substr(lastNl+1);
      }
      if(carry->size()>64*1024)carry.reset();
    }
#if !defined(_WIN32)
  }catch(...){::close(fd);throw;}
  ::close(fd);
#endif
  if(carry&&!carry->empty())for(const auto marker:metadataMarkers)
    if(carry->find(marker)!=std::string::npos){
      metadataLines.push_back(value::decodeUtf8(*carry));break;}
  return metadataLines;
}
} // namespace batbox::flights::scanning
