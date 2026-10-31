#include "instructions/project/async/AsyncDiscovery.hpp"
#include "shell/PosixPath.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::instructions::project::async {
namespace {
namespace fs=std::filesystem;
std::string relativePath(const std::string& base,const std::string& target){
  auto value=fs::path(target).lexically_relative(base).string();
  return value=="."?"":value;
}
}
Deferred<MemoryFiles> processConditionedMdRules(event_loop::Loop& loop,
  const std::string& targetPath,const std::string& rulesDir,
  const std::string& type,std::shared_ptr<PathSet> processedPaths,
  bool includeExternal,const Services& services){
  if(!loop.isOwnerThread())throw std::logic_error("Conditional rule owner required");
  Deferred<MemoryFiles> output(loop);
  try{
    processMdRules(loop,rulesDir,type,processedPaths,includeExternal,true,
      services).then([output,targetPath,rulesDir,type,sync=services.sync](
      const MemoryFiles* files,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        MemoryFiles selected;
        for(const auto& file:*files){
          if(!file.globs||file.globs->empty())continue;
          std::string baseDir;
          if(type=="Project")
            baseDir=fs::path(rulesDir).parent_path().parent_path().string();
          else{
            if(!sync.originalCwd)
              throw std::invalid_argument("Original instruction CWD import required");
            baseDir=sync.originalCwd();
          }
          const auto relative=fs::path(targetPath).is_absolute()
            ?relativePath(baseDir,targetPath):targetPath;
          if(relative.empty()||relative.starts_with("..")||
            fs::path(relative).is_absolute())continue;
          if(!sync.matchRuleIgnore)
            throw std::invalid_argument("ignore matcher import required");
          if(sync.matchRuleIgnore(relative,*file.globs))selected.push_back(file);
        }
        output.resolve(std::move(selected));
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
Deferred<MemoryFiles> getConditionalRulesForCwdLevelDirectory(
  event_loop::Loop& loop,const std::string& dir,const std::string& targetPath,
  std::shared_ptr<PathSet> processedPaths,const Services& services){
  if(!loop.isOwnerThread())throw std::logic_error("CWD-level rule owner required");
  Deferred<MemoryFiles> output(loop);
  try{return event_loop::adoptAsyncReturn(processConditionedMdRules(loop,
    targetPath,shell::joinPosixPath(shell::joinPosixPath(dir,".batbox"),"rules"),
    "Project",std::move(processedPaths),false,services));}
  catch(...){output.reject(std::current_exception());return output;}
}
} // namespace batbox::instructions::project::async
