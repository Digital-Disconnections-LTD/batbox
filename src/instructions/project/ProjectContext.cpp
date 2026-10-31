#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::project {
namespace {
constexpr const char* instructionPoetry=
  "Codebase and user instructions are shown below. Be sure to adhere to "
  "these instructions. IMPORTANT: These instructions OVERRIDE any default "
  "behavior and you MUST follow them exactly as written.";
bool required(const std::function<bool()>& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Missing instruction binding: ")+name);
  return callback();
}
}
std::vector<MemoryFileInfo> getLargeMemoryFiles(
  const std::vector<MemoryFileInfo>& files){
  std::vector<MemoryFileInfo> result;
  for(const auto& file:files)
    if(value::utf16CodeUnits(file.content).size()>40000)result.push_back(file);
  return result;
}
std::vector<MemoryFileInfo> filterInjectedMemoryFiles(
  const std::vector<MemoryFileInfo>& files,const DiscoveryServices& services){
  if(!required(services.skipAutoMemoryIndex,"skipAutoMemoryIndex"))return files;
  std::vector<MemoryFileInfo> result;
  for(const auto& file:files)
    if(file.type!="AutoMem"&&file.type!="TeamMem")result.push_back(file);
  return result;
}
std::string getBatboxMds(const std::vector<MemoryFileInfo>& files,
  const DiscoveryServices& services,
  const std::function<bool(const std::string&)>& filter){
  const bool skipProject=required(services.skipProjectInstructions,
    "skipProjectInstructions");
  std::vector<std::string> memories;
  for(const auto& file:files){
    if(filter&&!filter(file.type))continue;
    if(skipProject&&(file.type=="Project"||file.type=="Local"))continue;
    if(file.content.empty())continue;
    std::string description;
    if(file.type=="Project")description=" (project instructions, checked into the codebase)";
    else if(file.type=="Local")
      description=" (user's private project instructions, not checked in)";
    else if(required(services.teamMemoryFeature,"teamMemoryFeature")&&file.type=="TeamMem")
      description=" (shared team memory, synced across the organization)";
    else if(file.type=="AutoMem")
      description=" (user's auto-memory, persists across conversations)";
    else description=" (user's private global instructions for all projects)";
    const auto content=value::trim(file.content);
    if(required(services.teamMemoryFeature,"teamMemoryFeature")&&file.type=="TeamMem")
      memories.push_back("Contents of "+file.path+description+
        ":\n\n<team-memory-content source=\"shared\">\n"+content+
        "\n</team-memory-content>");
    else memories.push_back("Contents of "+file.path+description+":\n\n"+content);
  }
  if(memories.empty())return {};
  std::string poetry=std::string(instructionPoetry)+"\n\n";
  for(std::size_t i=0;i<memories.size();++i){
    if(i)poetry+="\n\n";
    poetry+=memories[i];
  }
  return poetry;
}
}
