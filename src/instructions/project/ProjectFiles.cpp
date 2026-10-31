#include "instructions/project/ProjectDiscovery.hpp"
#include "filesystem/FileOps.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <unicode/ustring.h>

namespace batbox::instructions::project {
namespace {
std::string decodeUtf8WithReplacement(const std::string& bytes){
  UErrorCode error=U_ZERO_ERROR;
  int32_t unitsLength=0;
  u_strFromUTF8WithSub(nullptr,0,&unitsLength,bytes.data(),
    static_cast<int32_t>(bytes.size()),0xFFFD,nullptr,&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("UTF-8 instruction decoding failed");
  error=U_ZERO_ERROR;
  std::vector<UChar> units(static_cast<std::size_t>(unitsLength)+1);
  u_strFromUTF8WithSub(units.data(),static_cast<int32_t>(units.size()),
    &unitsLength,bytes.data(),static_cast<int32_t>(bytes.size()),
    0xFFFD,nullptr,&error);
  if(U_FAILURE(error))throw std::runtime_error("UTF-8 instruction decoding failed");
  int32_t utf8Length=0;
  error=U_ZERO_ERROR;
  u_strToUTF8(nullptr,0,&utf8Length,units.data(),unitsLength,&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("UTF-8 instruction decoding failed");
  error=U_ZERO_ERROR;
  std::string decoded(static_cast<std::size_t>(utf8Length),'\0');
  u_strToUTF8(decoded.data(),utf8Length,&utf8Length,units.data(),
    unitsLength,&error);
  if(U_FAILURE(error))throw std::runtime_error("UTF-8 instruction decoding failed");
  return decoded;
}
}
ParsedMemoryContent safelyReadMemoryFileAsync(const std::string& path,
  const std::string& type,const std::optional<std::string>& includeBasePath,
  const DiscoveryServices& services){
  try{
    std::ifstream input(path,std::ios::binary);
    if(!input) return {};
    const std::string bytes(std::istreambuf_iterator<char>{input},{});
    if(input.bad())return {};
    return parseMemoryFileContent(decodeUtf8WithReplacement(bytes),path,type,
      includeBasePath,services.parse);
  }catch(...){
    // Ignore individual read and parse failures during discovery.
    return {};
  }
}
std::vector<MemoryFileInfo> processMemoryFile(const std::string& path,
  const std::string& type,PathSet& processedPaths,bool includeExternal,
  const DiscoveryServices& services,int depth,
  const std::optional<std::string>& parent){
  const auto normalized=normalizePathForComparison(path,services);
  if(processedPaths.contains(normalized)||depth>=5)return {};
  if(isBatboxMdExcluded(path,type,services))return {};
  const auto resolved=filesystem::safeResolvePath(path);
  processedPaths.insert(normalized);
  if(resolved.isSymlink)
    processedPaths.insert(normalizePathForComparison(resolved.resolvedPath,services));
  auto parsed=safelyReadMemoryFileAsync(path,type,resolved.resolvedPath,services);
  if(!parsed.info||value::trim(parsed.info->content).empty())return {};
  if(parent&& !parent->empty())parsed.info->parent=*parent;
  std::vector<MemoryFileInfo> result;
  result.push_back(std::move(*parsed.info));
  for(const auto& includedPath:parsed.includePaths){
    if(!services.originalCwd||!services.pathInWorkingPath)
      throw std::invalid_argument("Missing instruction include path binding");
    const bool external=!services.pathInWorkingPath(includedPath,
      services.originalCwd());
    if(external&&!includeExternal)continue;
    auto included=processMemoryFile(includedPath,type,processedPaths,
      includeExternal,services,depth+1,path);
    result.insert(result.end(),std::make_move_iterator(included.begin()),
      std::make_move_iterator(included.end()));
  }
  return result;
}
}
