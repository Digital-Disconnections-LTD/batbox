#include "instructions/project/ProjectDiscovery.hpp"
#include "filesystem/FileOps.hpp"
#include <filesystem>
#include <iterator>

namespace batbox::instructions::project {
namespace fs=std::filesystem;
std::vector<MemoryFileInfo> processMdRules(const std::string& rulesDir,
  const std::string& type,PathSet& processedPaths,bool includeExternal,
  bool conditionalRule,const DiscoveryServices& services,
  PathSet* visitedDirs){
  PathSet ownedVisited;
  if(!visitedDirs)visitedDirs=&ownedVisited;
  if(visitedDirs->contains(rulesDir))return {};
  try{
    const auto resolved=filesystem::safeResolvePath(rulesDir);
    visitedDirs->insert(rulesDir);
    if(resolved.isSymlink)visitedDirs->insert(resolved.resolvedPath);
    std::vector<MemoryFileInfo> result;
    std::vector<fs::directory_entry> entries;
    try{
      for(const auto& entry:fs::directory_iterator(resolved.resolvedPath))
        entries.push_back(entry);
    }catch(const fs::filesystem_error& error){
      const auto code=error.code();
      if(code==std::errc::no_such_file_or_directory||
         code==std::errc::permission_denied||
         code==std::errc::not_a_directory)return {};
      throw;
    }
    for(const auto& entry:entries){
      const auto entryPath=(fs::path(rulesDir)/entry.path().filename()).string();
      const auto resolvedEntry=filesystem::safeResolvePath(entryPath);
      const auto status=resolvedEntry.isSymlink
        ?fs::status(resolvedEntry.resolvedPath):entry.symlink_status();
      if(fs::is_directory(status)){
        auto children=processMdRules(resolvedEntry.resolvedPath,type,
          processedPaths,includeExternal,conditionalRule,services,visitedDirs);
        result.insert(result.end(),std::make_move_iterator(children.begin()),
          std::make_move_iterator(children.end()));
      }else if(fs::is_regular_file(status)&&
               entry.path().filename().string().ends_with(".md")){
        auto files=processMemoryFile(resolvedEntry.resolvedPath,type,
          processedPaths,includeExternal,services);
        for(auto& file:files){
          if(conditionalRule?file.globs.has_value():!file.globs.has_value())
            result.push_back(std::move(file));
        }
      }
    }
    return result;
  }catch(...){
    // Ignore traversal errors and continue collecting accessible rules.
    return {};
  }
}
}
