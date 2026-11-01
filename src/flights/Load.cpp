#include "flights/Load.hpp"
#include "flights/IndigenousPath.hpp"
#include "flights/LoadParsing.hpp"
#include <fstream>
#include <filesystem>
#include <system_error>
namespace batbox::flights {
void bindIndigenousFileReads(LoadServices& services) {
  services.stat=[](const std::string& path)->std::uintmax_t{
    return std::filesystem::file_size(indigenousPath(path));
  };
  services.readFile=[](const std::string& path){
    const auto file=indigenousPath(path);
    std::ifstream input(file,std::ios::binary);
    if(!input)throw std::system_error(errno,std::generic_category(),"readFile");
    // Size the buffer once instead of growing it byte by byte; keep reading
    // in case the file grew after it was measured.
    std::error_code sizeError;
    const auto expected=std::filesystem::file_size(file,sizeError);
    std::string raw;
    if(!sizeError)raw.reserve(static_cast<std::size_t>(expected));
    char chunk[1<<16];
    while(input.read(chunk,sizeof chunk)||input.gcount()>0)
      raw.append(chunk,static_cast<std::size_t>(input.gcount()));
    if(input.bad())throw std::system_error(errno,std::generic_category(),"readFile");
    return raw;
  };
}
LoadedTranscript loadTranscriptFile(const std::string& path,bool keepAllLeaves,LoadServices& services) {
  LoadedTranscript out;
  try {
    std::optional<std::string> buffer;std::vector<std::string> metadata;bool preserved=false;
    if(!services.disablePrecompactSkip) {
      const auto size=services.stat(path);
      if(size>services.skipThreshold) {
        auto scan=services.readTranscriptForLoad(path,size);buffer=std::move(scan.postBoundaryBuf);preserved=scan.hasPreservedSegment;
        if(scan.boundaryStartOffset>0)metadata=services.scanPreBoundaryMetadata(path,scan.boundaryStartOffset);
      }
    }
    if(!buffer)buffer=services.readFile(path);
    if(!keepAllLeaves && !preserved && !services.disablePrecompactSkip && buffer->size()>services.skipThreshold)
      buffer=services.walkChainBeforeParse(*buffer);
    parseLoadedTranscriptBuffers(*buffer,metadata,out);
  }catch(const std::exception&){} // Preserve maps populated before a read or parse failure.
  finishLoadedTranscript(out,services.pruneLeaves);
  return out;
}
std::optional<Json> getBatTranscript(const std::string& bat,Paths& paths,LoadServices& services,GraphServices& graph) {
  const auto file=paths.getBatTranscriptPath(bat);
  try {
    return selectBatTranscript(loadTranscriptFile(file,false,services),bat,graph);
  }catch(const std::exception&){return std::nullopt;}
}
}
