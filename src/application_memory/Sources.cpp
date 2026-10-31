#include "application_memory/Provider.hpp"
#include "application_memory/Consolidation.hpp"
#include "application_memory/IndigenousWork.hpp"
#include "application/Async.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

namespace batbox::application_memory {
namespace {
using Unit=event_loop::Unit;
Json source(const std::string& path,const std::string& label,
    const std::string& type,const std::string& scope,bool directory,
    const std::string& description={}){
  std::error_code error;
  return {{"path",path},{"label",label},{"type",type},{"scope",scope},
    {"exists",std::filesystem::exists(runtime::pathFromUtf8(path),error)},
    {"directory",directory},{"description",description}};
}
}
event_loop::Deferred<Json> Provider::listSources(){
  requireOwner();
  event_loop::Deferred<Json> output(inputs_.owner);
  auto self=shared_from_this();
  inputs_.poetry->memoryFiles().then([self,output](const context::MemoryFiles* files,
      const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    try{
      Json rows=Json::array();std::set<std::string> seen;
      if(files)for(const auto& file:*files){
        if(!seen.insert(file.path).second)continue;
        rows.push_back(source(file.path,file.type+" memory",file.type,
          file.type=="User"?"user":file.type=="Local"?"local":"project",false,
          "Loaded instructions"));
      }
      const auto addMissing=[&](const std::filesystem::path& path,
          const std::string& type,const std::string& scope){
        const auto text=runtime::pathToUtf8(path);
        if(seen.insert(text).second)rows.push_back(source(text,type+" memory",
          type,scope,false,"Instructions"));
      };
      addMissing(application::configurationDirectory(self->inputs_.config)/
        "BATBOX.md","User","user");
      addMissing(self->inputs_.config.cwd/"BATBOX.md","Project","project");
      auto dirs=instructions::isAutoMemoryEnabled(self->paths())?
        self->memoryDirectories():std::vector<std::pair<std::string,std::string>>{};
      auto pending=memoryIO<Json>(self->inputs_.owner,
        [rows=std::move(rows),seen=std::move(seen),dirs=std::move(dirs)]()mutable{
          for(std::size_t i=0;i<dirs.size();++i){
            const auto& [directory,scope]=dirs[i];
            const auto label=i==0?std::string("Auto memory"):
              "Agent memory: "+runtime::pathToUtf8(runtime::pathFromUtf8(directory).filename());
            rows.push_back(source(directory,label+" folder","Folder",scope,true));
            const auto entry=runtime::pathToUtf8(runtime::pathFromUtf8(directory)/"MEMORY.md");
            if(seen.insert(entry).second)
              rows.push_back(source(entry,label+" index","AutoMem",scope,false,
                "Loaded index; topic files carry the durable memories"));
            for(const auto& memory:instructions::scanMemoryFiles(directory,{})){
              if(!seen.insert(memory.filePath).second)continue;
              const auto description=memory.description.is_string()?
                memory.description.get<std::string>():std::string{};
              auto row=source(memory.filePath,memory.filename,"Memory",scope,false,description);
              row["mtimeMs"]=memory.mtimeMs;
              if(memory.type)row["memoryType"]=*memory.type;
              rows.push_back(std::move(row));
            }
          }
          return rows;
        });
      pending.then([output](const Json* rows,const std::exception_ptr& failure){
        if(failure)output.reject(failure);else output.resolve(*rows);
      });
    }catch(...){output.reject(std::current_exception());}
  });
  return output;
}
event_loop::Deferred<std::string> Provider::read(const std::string& path){
  requireOwner();
  return memoryIO<std::string>(inputs_.owner,[path]{
    std::ifstream file(runtime::pathFromUtf8(path),std::ios::binary);
    if(!file)throw std::runtime_error("Cannot read memory file: "+path);
    return value::decodeUtf8(std::string(std::istreambuf_iterator<char>(file),{}),false);
  });
}
event_loop::Deferred<Unit> Provider::prepareForEdit(const std::string& path){
  requireOwner();
  return memoryIO<Unit>(inputs_.owner,[path]{
    const auto file=runtime::pathFromUtf8(path);
    std::filesystem::create_directories(file.parent_path());
    // Append mode creates a missing file and preserves existing bytes, even
    // if an editor or another flight created it after selection.
    std::ofstream output(file,std::ios::binary|std::ios::app);
    if(!output)throw std::runtime_error("Cannot prepare memory file: "+path);
    return Unit{};
  });
}
event_loop::Deferred<Unit> Provider::write(const std::string& path,
    const std::string& content){
  requireOwner();
  auto self=shared_from_this();event_loop::Deferred<Unit> output(inputs_.owner);
  memoryIO<Unit>(inputs_.owner,[path,content]{
    const auto file=runtime::pathFromUtf8(path);
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file,std::ios::binary|std::ios::trunc);
    if(!stream)throw std::runtime_error("Cannot write memory file: "+path);
    stream.write(content.data(),static_cast<std::streamsize>(content.size()));
    stream.close();
    if(!stream)throw std::runtime_error("Cannot finish memory write: "+path);
    return Unit{};
  }).then([self,output](const Unit*,const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    try{self->refresh();output.resolve({});}
    catch(...){output.reject(std::current_exception());}
  });
  return output;
}
event_loop::Deferred<Json> Provider::status(){
  requireOwner();
  auto directory=autoMemoryDirectory();
  Json value={{"enabled",instructions::isAutoMemoryEnabled(paths())},
    {"recallEnabled",recallEnabled()},{"autoDreamEnabled",dreamEnabled()},
    {"running",activeDream_.has_value()},{"directory",directory},
    {"lastResult",lastResult_}};
  return memoryIO<Json>(inputs_.owner,[value,directory]()mutable{
    value["lastConsolidatedAt"]=readLastConsolidatedAt(directory);
    return value;
  });
}
} // namespace batbox::application_memory
