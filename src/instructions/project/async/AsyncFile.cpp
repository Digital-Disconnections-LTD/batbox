#include "instructions/project/async/AsyncDiscovery.hpp"
#include "filesystem/FileOps.hpp"
#include "runtime/JsonValue.hpp"
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::instructions::project::async {
Deferred<ParsedMemoryContent> safelyReadMemoryFile(event_loop::Loop& loop,
  const std::string& path,const std::string& type,
  const std::optional<std::string>& includeBasePath,const Services& services){
  Deferred<ParsedMemoryContent> output(loop);
  try{
    if(!services.readFile)throw std::invalid_argument("Instruction readFile import required");
    services.readFile(path).then([output,path,type,includeBasePath,parse=services.sync.parse](
      const std::string* raw,const std::exception_ptr& error){
      if(error){output.resolve({});return;}
      try{output.resolve(parseMemoryFileContent(*raw,path,type,includeBasePath,parse));}
      catch(...){output.resolve({});}
    });
  }catch(...){output.resolve({});}
  return output;
}
namespace {
struct FileRun:std::enable_shared_from_this<FileRun> {
  event_loop::Loop& loop;
  std::string path,type;
  std::shared_ptr<PathSet> processed;
  bool includeExternal;
  Services services;
  int depth;
  std::optional<std::string> parent;
  Deferred<MemoryFiles> output;
  MemoryFiles result;
  std::vector<std::string> includes;
  std::size_t index=0;
  FileRun(event_loop::Loop& owner,std::string file,std::string fileType,
    std::shared_ptr<PathSet> paths,bool external,Services imports,int level,
    std::optional<std::string> parentPath):loop(owner),path(std::move(file)),
    type(std::move(fileType)),processed(std::move(paths)),includeExternal(external),
    services(std::move(imports)),depth(level),parent(std::move(parentPath)),output(owner){}
  void start(){
    try{
      const auto normalized=normalizePathForComparison(path,services.sync);
      if(processed->contains(normalized)||depth>=5){output.resolve({});return;}
      if(isBatboxMdExcluded(path,type,services.sync)){output.resolve({});return;}
      const auto resolved=filesystem::safeResolvePath(path);
      processed->insert(normalized);
      if(resolved.isSymlink)processed->insert(
        normalizePathForComparison(resolved.resolvedPath,services.sync));
      safelyReadMemoryFile(loop,path,type,resolved.resolvedPath,services).then(
        [self=shared_from_this()](const ParsedMemoryContent* parsed,
          const std::exception_ptr& error){
          if(error){self->output.reject(error);return;}
          try{
            if(!parsed->info||value::trim(parsed->info->content).empty()){
              self->output.resolve({});return;}
            auto file=*parsed->info;
            if(self->parent&&!self->parent->empty())file.parent=self->parent;
            self->result.push_back(std::move(file));
            self->includes=parsed->includePaths;
            self->next();
          }catch(...){self->output.reject(std::current_exception());}
        });
    }catch(...){output.reject(std::current_exception());}
  }
  void next(){
    try{
      while(index<includes.size()){
        const auto includedPath=includes[index++];
        if(!services.sync.originalCwd||!services.sync.pathInWorkingPath)
          throw std::invalid_argument("Instruction include path imports required");
        const bool external=!services.sync.pathInWorkingPath(includedPath,
          services.sync.originalCwd());
        if(external&&!includeExternal)continue;
        processMemoryFile(loop,includedPath,type,processed,includeExternal,services,
          depth+1,path).then([self=shared_from_this()](const MemoryFiles* files,
            const std::exception_ptr& error){
            if(error){self->output.reject(error);return;}
            try{
              self->result.insert(self->result.end(),files->begin(),files->end());
              self->next();
            }catch(...){self->output.reject(std::current_exception());}
          });
        return;
      }
      output.resolve(std::move(result));
    }catch(...){output.reject(std::current_exception());}
  }
};
}
Deferred<MemoryFiles> processMemoryFile(event_loop::Loop& loop,
  const std::string& path,const std::string& type,
  std::shared_ptr<PathSet> processedPaths,bool includeExternal,
  const Services& services,int depth,const std::optional<std::string>& parent){
  if(!loop.isOwnerThread())throw std::logic_error("Instruction file owner required");
  if(!processedPaths)throw std::invalid_argument("Processed instruction paths required");
  auto run=std::make_shared<FileRun>(loop,path,type,std::move(processedPaths),
    includeExternal,services,depth,parent);
  run->start();return run->output;
}
} // namespace batbox::instructions::project::async
