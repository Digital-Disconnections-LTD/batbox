#include "instructions/project/async/AsyncDiscovery.hpp"
#include "filesystem/FileOps.hpp"
#include "shell/PosixPath.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::instructions::project::async {
namespace {
struct RulesRun:std::enable_shared_from_this<RulesRun> {
  event_loop::Loop& loop;
  std::string dir,type;
  std::shared_ptr<PathSet> processed,visited;
  bool includeExternal,conditionalRule;
  Services services;
  Deferred<MemoryFiles> output;
  MemoryFiles result;
  std::vector<Dirent> entries;
  std::size_t index=0;
  RulesRun(event_loop::Loop& owner,std::string path,std::string fileType,
    std::shared_ptr<PathSet> paths,bool external,bool conditional,
    Services imports,std::shared_ptr<PathSet> directories):
    loop(owner),dir(std::move(path)),type(std::move(fileType)),
    processed(std::move(paths)),visited(std::move(directories)),
    includeExternal(external),conditionalRule(conditional),
    services(std::move(imports)),output(owner){}
  void empty(){output.resolve({});}
  void start(){
    if(visited->contains(dir)){empty();return;}
    try{
      const auto resolved=filesystem::safeResolvePath(dir);
      visited->insert(dir);
      if(resolved.isSymlink)visited->insert(resolved.resolvedPath);
      if(!services.readdir)throw std::invalid_argument("Instruction readdir import required");
      services.readdir(resolved.resolvedPath).then([self=shared_from_this()](
        const std::vector<Dirent>* values,const std::exception_ptr& error){
        if(error){self->empty();return;}
        try{self->entries=*values;self->next();}catch(...){self->empty();}
      });
    }catch(...){empty();}
  }
  void next(){
    try{
      while(index<entries.size()){
        const auto entry=entries[index++];
        const auto entryPath=shell::joinPosixPath(dir,entry.name);
        const auto resolved=filesystem::safeResolvePath(entryPath);
        if(resolved.isSymlink){
          if(!services.stat)throw std::invalid_argument("Instruction stat import required");
          services.stat(resolved.resolvedPath).then(
            [self=shared_from_this(),entry,path=resolved.resolvedPath](
              const FileKind* kind,const std::exception_ptr& error){
              if(error){self->empty();return;}
              self->processEntry(entry,path,kind->isDirectory,kind->isFile);
            });
          return;
        }
        if(entry.isDirectory||(entry.isFile&&entry.name.ends_with(".md"))){
          processEntry(entry,resolved.resolvedPath,entry.isDirectory,entry.isFile);
          return;
        }
      }
      output.resolve(std::move(result));
    }catch(...){empty();}
  }
  void processEntry(const Dirent& entry,const std::string& resolved,
    bool isDirectory,bool isFile){
    try{
      if(isDirectory){
        processMdRules(loop,resolved,type,processed,includeExternal,
          conditionalRule,services,visited).then([self=shared_from_this()](
          const MemoryFiles* files,const std::exception_ptr& error){
          if(error){self->empty();return;}
          try{self->result.insert(self->result.end(),files->begin(),files->end());
            self->next();}catch(...){self->empty();}
        });
        return;
      }
      if(isFile&&entry.name.ends_with(".md")){
        processMemoryFile(loop,resolved,type,processed,includeExternal,services).then(
          [self=shared_from_this()](const MemoryFiles* files,
            const std::exception_ptr& error){
            if(error){self->empty();return;}
            try{
              for(const auto& file:*files){
                if(self->conditionalRule?file.globs.has_value():!file.globs.has_value())
                  self->result.push_back(file);
              }
              self->next();
            }catch(...){self->empty();}
          });
        return;
      }
      next();
    }catch(...){empty();}
  }
};
}
Deferred<MemoryFiles> processMdRules(event_loop::Loop& loop,
  const std::string& rulesDir,const std::string& type,
  std::shared_ptr<PathSet> processedPaths,bool includeExternal,
  bool conditionalRule,const Services& services,
  std::shared_ptr<PathSet> visitedDirs){
  if(!loop.isOwnerThread())throw std::logic_error("Instruction rules owner required");
  if(!processedPaths)throw std::invalid_argument("Processed instruction paths required");
  if(!visitedDirs)visitedDirs=std::make_shared<PathSet>();
  auto run=std::make_shared<RulesRun>(loop,rulesDir,type,std::move(processedPaths),
    includeExternal,conditionalRule,services,std::move(visitedDirs));
  run->start();return run->output;
}
} // namespace batbox::instructions::project::async
