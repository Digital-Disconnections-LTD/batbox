#include "gadgets/files/Glob.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
namespace fs=std::filesystem;
namespace {
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing Glob binding: ")+name);
}
} // namespace

GlobBase extractGlobBaseDirectory(const std::string& pattern,const GlobServices& services){
  const auto special=pattern.find_first_of("*?[{");
  if(special==std::string::npos)
    return {fs::path(pattern).parent_path().empty()?".":fs::path(pattern).parent_path().string(),
      fs::path(pattern).filename().string()};
  const auto prefix=pattern.substr(0,special);
  const auto separator=prefix.find_last_of("/");
  if(separator==std::string::npos)return {"",pattern};
  auto base=prefix.substr(0,separator);
  const auto relative=pattern.substr(separator+1);
  if(base.empty()&&separator==0)base="/";
  if(services.platform&&services.platform()=="windows"&&base.size()==2&&
     std::isalpha(static_cast<unsigned char>(base[0]))&&base[1]==':')base+='/';
  return {base,relative};
}

std::string toRelativePath(const std::string& absolutePath,const std::string& cwd){
  const auto relative=fs::path(absolutePath).lexically_relative(fs::path(cwd)).string();
  return relative.starts_with("..") ? absolutePath : relative;
}

GlobResult glob(const std::string& filePattern,const std::string& cwd,
                std::size_t limit,std::size_t offset,const Json& permission,
                std::stop_token stopToken,const GlobServices& services){
  std::string searchDir=cwd,searchPattern=filePattern;
  if(fs::path(filePattern).is_absolute()){
    const auto base=extractGlobBaseDirectory(filePattern,services);
    if(!base.baseDir.empty()){searchDir=base.baseDir;searchPattern=base.relativePattern;}
  }
  require(static_cast<bool>(services.getFileReadIgnorePatterns),"getFileReadIgnorePatterns");
  require(static_cast<bool>(services.normalizePatternsToPath),"normalizePatternsToPath");
  const auto ignore=services.normalizePatternsToPath(
    services.getFileReadIgnorePatterns(permission),searchDir);
  require(static_cast<bool>(services.noIgnore),"noIgnore");
  require(static_cast<bool>(services.hidden),"hidden");
  // Newest first, so a result cut at the limit keeps the recently changed files.
  std::vector<std::string> args={"--files","--glob",searchPattern,"--sortr=modified"};
  if(services.noIgnore())args.push_back("--no-ignore");
  if(services.hidden())args.push_back("--hidden");
  for(const auto& pattern:ignore){args.push_back("--glob");args.push_back("!"+pattern);}
  require(static_cast<bool>(services.getGlobExclusionsForPluginStash),"getGlobExclusionsForPluginCache");
  for(const auto& exclusion:services.getGlobExclusionsForPluginStash(searchDir)){
    args.push_back("--glob");args.push_back(exclusion);
  }
  require(static_cast<bool>(services.ripGrep),"ripGrep");
  auto found=services.ripGrep(args,searchDir,stopToken);
  const auto& all=found.lines;
  const bool truncated=all.size()>offset+limit;
  const auto begin=std::min(offset,all.size());
  const auto end=std::min(offset+limit,all.size());
  std::vector<std::string> files;
  files.reserve(end-begin);
  for(auto at=begin;at<end;++at)files.push_back(fs::path(all[at]).is_absolute()
    ? all[at] : (fs::path(searchDir)/all[at]).lexically_normal().string());
  return {std::move(files),truncated,std::move(found.incomplete)};
}
} // namespace batbox::gadgets::files
