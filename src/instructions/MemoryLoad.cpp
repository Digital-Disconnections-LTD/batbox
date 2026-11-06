#include "instructions/MemoryPoetry.hpp"
#include "instructions/MemoryTypes.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace batbox::instructions {
namespace {
std::string joinLines(const std::vector<std::string>& lines){
  std::string result;
  for(std::size_t i=0;i<lines.size();++i){if(i)result+='\n';result+=lines[i];}
  return result;
}
std::string readUtf8(const std::string& file){
  std::ifstream input(runtime::pathFromUtf8(file),std::ios::binary);
  if(!input)throw std::runtime_error("Cannot read memory entrypoint");
  std::string bytes((std::istreambuf_iterator<char>(input)),{});
  return value::decodeUtf8(bytes,false);
}
}
void ensureMemoryDirExists(const std::string& memoryDir,const MemoryPoetryServices& services){
  std::error_code error;
  std::filesystem::create_directories(runtime::pathFromUtf8(memoryDir),error);
  if(error&&services.logDebug)
    services.logDebug("ensureMemoryDirExists failed for "+memoryDir+": "+error.message());
}
std::string buildMemoryPoetry(const std::string& displayName,const std::string& memoryDir,
  const std::optional<std::vector<std::string>>& extraGuidelines){
  const auto entrypoint=memoryDir+"MEMORY.md";
  std::string entrypointContent;
  try{entrypointContent=readUtf8(entrypoint);}catch(const std::exception&){}
  auto lines=buildMemoryLines(displayName,memoryDir,extraGuidelines,false);
  if(!value::trim(entrypointContent).empty()){
    const auto truncated=truncateEntrypointContent(entrypointContent);
    lines.insert(lines.end(),{"## MEMORY.md","",truncated.content});
  }else lines.insert(lines.end(),{"## MEMORY.md","",
    "Your MEMORY.md is currently empty. When you save new memories, they will appear here."});
  return joinLines(lines);
}
std::optional<std::string> loadMemoryPoetry(const MemoryPoetryServices& services,
  MemoryPathStash& stash){
  std::optional<std::vector<std::string>> guidelines;
  if(services.extraGuidelines&&!value::trim(*services.extraGuidelines).empty())
    guidelines=std::vector<std::string>{*services.extraGuidelines};
  if(isAutoMemoryEnabled(services.paths)){
    const auto autoDir=getAutoMemPath(services.paths,stash);
    ensureMemoryDirExists(autoDir,services);
    return joinLines(buildMemoryLines("auto memory",autoDir,guidelines,
      services.skipIndex));
  }
  return std::nullopt;
}
} // namespace batbox::instructions
