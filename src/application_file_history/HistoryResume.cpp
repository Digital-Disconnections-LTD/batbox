#include "application_file_history/HistoryInternal.hpp"
#include "application/Async.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::application_file_history {
event_loop::Deferred<event_loop::Unit> History::copyForResume(const Json& log){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("File history resume requires owner loop");
  event_loop::Deferred<event_loop::Unit> result(inputs_.owner);
  if(!enabled()){result.resolve({});return result;}
  const auto& snapshots=value::property(log,"fileHistorySnapshots");
  const auto& messages=value::property(log,"messages");
  if(!snapshots.is_array()||snapshots.empty()||
      !messages.is_array()||messages.empty()){
    result.resolve({});return result;
  }
  const auto previous=detail::stringField(messages.back(),"sessionId");
  if(previous.empty()){
    report(std::make_exception_ptr(std::runtime_error(
      "FileHistory: Failed to copy backups on restore (no previous session id)")));
    result.resolve({});return result;
  }
  const auto current=inputs_.flight->id();
  if(previous==current){result.resolve({});return result;}
  auto self=shared_from_this();
  auto worker=application::runInWorker<std::vector<Json>>(
    inputs_.owner,[self,snapshots,previous,current]{
      std::vector<Json> successful;
      const auto targetDir=self->inputs_.configHome/"file-history"/current;
      try{std::filesystem::create_directories(targetDir);}
      catch(...){self->report(std::current_exception());return successful;}
      for(const auto& snapshot:snapshots){
        bool failed=false;
        const auto& backups=value::property(snapshot,"trackedFileBackups");
        if(backups.is_object())for(auto it=backups.begin();it!=backups.end();++it){
          const auto name=detail::stringField(it.value(),"backupFileName");
          if(name.empty())continue;
          const auto source=self->backupPath(name,previous);
          const auto target=targetDir/name;
          try{
            std::error_code error;
            std::filesystem::create_hard_link(source,target,error);
            if(error==std::errc::file_exists)continue;
            if(error){
              error.clear();
              std::filesystem::copy_file(source,target,
                std::filesystem::copy_options::overwrite_existing,error);
              if(error)throw std::filesystem::filesystem_error(
                "copy file history backup",source,target,error);
            }
          }catch(...){failed=true;self->report(std::current_exception());}
        }
        if(!failed)successful.push_back(snapshot);
      }
      return successful;
    });
  worker.then([self,result](const std::vector<Json>* snapshots,
      const std::exception_ptr& error){
    if(error){self->report(error);result.resolve({});return;}
    for(const auto& snapshot:*snapshots){
      const auto id=detail::stringField(snapshot,"messageId");
      if(!id.empty())self->record(id,snapshot,false);
    }
    result.resolve({});
  });
  return result;
}
} // namespace batbox::application_file_history
