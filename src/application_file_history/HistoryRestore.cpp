#include "application_file_history/HistoryInternal.hpp"
#include "application/Async.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::application_file_history {
std::vector<std::string> History::applySnapshot(const Json& state,
    const Json& target,const std::string& flightId){
  std::vector<std::string> changedFiles;
  for(const auto& path:detail::tracked(state)){
    try{
      const auto file=expandPath(path);
      const auto backup=backupAt(state,target,path);
      if(!backup){
        report(std::make_exception_ptr(std::runtime_error(
          "FileHistory: Error finding the backup file to apply")));
        continue;
      }
      if(!*backup){
        std::error_code error;
        const bool removed=std::filesystem::remove(file,error);
        if(error&&error!=std::errc::no_such_file_or_directory)
          throw std::filesystem::filesystem_error("delete rewound file",file,error);
        if(removed)changedFiles.push_back(runtime::pathToUtf8(file));
        continue;
      }
      if(changed(file,**backup,flightId)){
        if(restoreBackup(file,**backup,flightId))
          changedFiles.push_back(runtime::pathToUtf8(file));
        else report(std::make_exception_ptr(std::runtime_error(
          "FileHistory: [Rewind] Backup file not found: "+
          backupPath(**backup,flightId).string())));
      }
    }catch(...){report(std::current_exception());}
  }
  return changedFiles;
}
event_loop::Deferred<event_loop::Unit> History::rewind(
    const std::string& messageId){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("File history rewind requires owner loop");
  event_loop::Deferred<event_loop::Unit> result(inputs_.owner);
  if(!enabled()){result.resolve({});return result;}
  const auto captured=state();
  const auto snapshot=findSnapshot(captured,messageId);
  if(!snapshot){
    result.reject(std::make_exception_ptr(std::runtime_error(
      "The selected snapshot was not found")));
    return result;
  }
  auto self=shared_from_this();
  const auto flightId=inputs_.flight->id();
  auto worker=application::runInWorker<std::vector<std::string>>(
    inputs_.owner,[self,captured,target=*snapshot,flightId]{
      return self->applySnapshot(captured,target,flightId);
    });
  worker.then([result](const std::vector<std::string>*,
      const std::exception_ptr& error){
    if(error)result.reject(error);else result.resolve({});
  });
  return result;
}
event_loop::Deferred<bool> History::hasAnyChanges(
    const std::string& messageId){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("File history check requires owner loop");
  event_loop::Deferred<bool> result(inputs_.owner);
  if(!enabled()){result.resolve(false);return result;}
  const auto captured=state();
  const auto snapshot=findSnapshot(captured,messageId);
  if(!snapshot){result.resolve(false);return result;}
  auto self=shared_from_this();
  const auto flightId=inputs_.flight->id();
  return application::runInWorker<bool>(inputs_.owner,
    [self,captured,target=*snapshot,flightId]{
      for(const auto& path:detail::tracked(captured)){
        try{
          const auto file=self->expandPath(path);
          const auto backup=self->backupAt(captured,target,path);
          if(!backup)continue;
          if(!*backup){
            std::error_code error;
            const bool exists=std::filesystem::exists(file,error);
            if(!error&&exists)return true;
          }else if(self->changed(file,**backup,flightId))return true;
        }catch(...){self->report(std::current_exception());}
      }
      return false;
    });
}
} // namespace batbox::application_file_history
