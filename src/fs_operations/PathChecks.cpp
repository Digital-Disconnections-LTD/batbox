#include "fs_operations/PathChecks.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::fs_operations {
namespace {
bool unc(const std::string& path){
  return path.rfind("//",0)==0||path.rfind("\\\\",0)==0;
}
bool special(const Stats& stats){
  return stats.fifo||stats.socket||stats.characterDevice||stats.blockDevice;
}
void addUnique(std::vector<std::string>& paths,
    std::unordered_set<std::string>& seen,const std::string& path){
  if(seen.insert(path).second)paths.push_back(path);
}
void require(const FileView& fs,const PathImports& path){
  if(!fs.existsSync||!fs.lstatSync||!fs.realpathSync||!fs.readlinkSync||
      !path.dirname||!path.basename||!path.join||!path.isAbsolute||
      !path.resolve||!path.homedir||!path.normalizeNFC)
    throw std::invalid_argument("Filesystem path imports are required");
}
}

bool isDuplicatePath(const FileView& fs,const std::string& filePath,
    std::unordered_set<std::string>& loadedPaths){
  const auto resolved=filesystem::safeResolvePath(fs,filePath).resolvedPath;
  if(loadedPaths.contains(resolved))return true;
  loadedPaths.insert(resolved);
  return false;
}

std::optional<std::string> resolveDeepestExistingAncestorSync(
    const FileView& fs,const std::string& absolutePath,const PathImports& path){
  std::string dir=absolutePath;
  std::vector<std::string> segments;
  while(dir!=path.dirname(dir)){
    Stats stats;
    try{stats=fs.lstatSync(dir);}
    catch(...){
      segments.insert(segments.begin(),path.basename(dir));
      dir=path.dirname(dir);
      continue;
    }
    if(stats.symbolicLink){
      try{
        const auto resolved=fs.realpathSync(dir);
        if(segments.empty())return resolved;
        auto parts=segments;parts.insert(parts.begin(),resolved);
        return path.join(parts);
      }catch(...){
        const auto target=fs.readlinkSync(dir);
        const auto absoluteTarget=path.isAbsolute(target)?target:
          path.resolve(path.dirname(dir),target);
        if(segments.empty())return absoluteTarget;
        auto parts=segments;parts.insert(parts.begin(),absoluteTarget);
        return path.join(parts);
      }
    }
    try{
      const auto resolved=fs.realpathSync(dir);
      if(resolved!=dir){
        if(segments.empty())return resolved;
        auto parts=segments;parts.insert(parts.begin(),resolved);
        return path.join(parts);
      }
    }catch(...){/* Return undefined after realpath failure. */}
    return std::nullopt;
  }
  return std::nullopt;
}

std::vector<std::string> getPathsForPermissionCheck(
    const std::string& inputPath,const FileView& fs,const PathImports& path){
  require(fs,path);
  std::string candidate=inputPath;
  if(candidate=="~")candidate=path.normalizeNFC(path.homedir());
  else if(candidate.rfind("~/",0)==0)
    candidate=path.join({path.normalizeNFC(path.homedir()),candidate.substr(2)});
  std::vector<std::string> paths;
  std::unordered_set<std::string> seen;
  addUnique(paths,seen,candidate);
  if(unc(candidate))return paths;
  try{
    std::string currentPath=candidate;
    std::unordered_set<std::string> visited;
    constexpr int maxDepth=40;
    for(int depth=0;depth<maxDepth;++depth){
      if(visited.contains(currentPath))break;
      visited.insert(currentPath);
      if(!fs.existsSync(currentPath)){
        if(currentPath==candidate){
          const auto resolved=resolveDeepestExistingAncestorSync(fs,candidate,path);
          if(resolved)addUnique(paths,seen,*resolved);
        }
        break;
      }
      const auto stats=fs.lstatSync(currentPath);
      if(special(stats))break;
      if(!stats.symbolicLink)break;
      const auto target=fs.readlinkSync(currentPath);
      const auto absoluteTarget=path.isAbsolute(target)?target:
        path.resolve(path.dirname(currentPath),target);
      addUnique(paths,seen,absoluteTarget);
      currentPath=absoluteTarget;
    }
  }catch(...){/* Keep paths already collected. */}
  const auto resolved=filesystem::safeResolvePath(fs,candidate);
  if(resolved.isSymlink&&resolved.resolvedPath!=candidate)
    addUnique(paths,seen,resolved.resolvedPath);
  return paths;
}
}
