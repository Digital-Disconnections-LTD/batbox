#include "application_file_history/HistoryInternal.hpp"
#include "application/Async.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_file_history {
namespace {
event_loop::Deferred<event_loop::Unit> ready(event_loop::Loop& owner){
  event_loop::Deferred<event_loop::Unit> result(owner);result.resolve({});return result;
}
void notifyChanges(History& history,const Inputs& inputs,
    const Json& previous,const Json& current,const Json& state){
  if(!inputs.notifyEditorFileUpdated)return;
  const auto& oldBackups=value::property(previous,"trackedFileBackups");
  const auto& newBackups=value::property(current,"trackedFileBackups");
  for(const auto& path:detail::tracked(state)){
    const auto& before=value::property(oldBackups,path);
    const auto& after=value::property(newBackups,path);
    if(value::property(before,"backupFileName")==
        value::property(after,"backupFileName")&&
        value::property(before,"version")==value::property(after,"version"))continue;
    std::optional<std::string> oldContent,newContent;
    const auto oldName=detail::stringField(before,"backupFileName");
    const auto newName=detail::stringField(after,"backupFileName");
    if(!oldName.empty())oldContent=detail::readOrNull(history.backupPath(oldName));
    if(!newName.empty())newContent=detail::readOrNull(history.backupPath(newName));
    if(oldContent!=newContent)
      inputs.notifyEditorFileUpdated(inputs.originalCwd/runtime::pathFromUtf8(path),
        oldContent,newContent);
  }
}
}
void History::trackEdit(const std::string& filePath,
    const std::string& messageId){
  if(!inputs_.owner.isOwnerThread()){
    auto self=shared_from_this();
    application::onOwnerAndWait<void>(inputs_.owner.dispatcher(),
      [self,filePath,messageId]{self->trackEdit(filePath,messageId);});
    return;
  }
  if(!enabled())return;
  const auto indigenous=runtime::pathFromUtf8(filePath);
  const auto key=trackingPath(indigenous);
  const auto captured=state();
  const auto snapshots=detail::snapshots(captured);
  if(snapshots.empty()){
    report(std::make_exception_ptr(std::runtime_error(
      "FileHistory: Missing most recent snapshot")));
    return;
  }
  const auto& prior=snapshots.back();
  if(value::property(value::property(prior,"trackedFileBackups"),key).is_object())
    return;
  Json backup;
  try{backup=createBackup(indigenous,1,inputs_.flight->id());}
  catch(...){report(std::current_exception());return;}
  auto current=state();
  auto entries=detail::snapshots(current);
  if(entries.empty()||value::property(value::property(entries.back(),
      "trackedFileBackups"),key).is_object())return;
  Json last=entries.back();
  if(!last.contains("trackedFileBackups")||
      !last["trackedFileBackups"].is_object())
    last["trackedFileBackups"]=Json::object();
  last["trackedFileBackups"][key]=backup;
  entries.back()=last;
  current["snapshots"]=std::move(entries);
  Json tracked=value::property(current,"trackedFiles");
  if(!tracked.is_object()){
    tracked=Json::object();
    for(const auto& path:detail::tracked(current))tracked[path]=true;
  }
  tracked[key]=true;current["trackedFiles"]=std::move(tracked);
  setState(std::move(current));
  record(messageId,last,true);
}
event_loop::Deferred<event_loop::Unit> History::makeSnapshot(
    const std::string& messageId){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("File history snapshot requires owner loop");
  if(!enabled())return ready(inputs_.owner);
  event_loop::Deferred<event_loop::Unit> result(inputs_.owner);
  const auto captured=state();
  if(detail::snapshots(captured).empty()){
    // The first snapshot has no asynchronous backup work. Publish it
    // before a fast model/gadget turn can reach trackEdit.
    Json snapshot={{"messageId",messageId},
      {"trackedFileBackups",Json::object()},
      {"timestamp",platform::isoTimestamp()}};
    Json current=captured;
    current["snapshots"]=Json::array({snapshot});
    const auto& sequence=value::property(current,"snapshotSequence");
    current["snapshotSequence"]=(sequence.is_number_integer()?
      sequence.get<std::int64_t>():0)+1;
    setState(current);record(messageId,snapshot,false);
    result.resolve({});return result;
  }
  const auto flightId=inputs_.flight->id();
  auto self=shared_from_this();
  auto worker=application::runInWorker<Json>(inputs_.owner,
      [self,captured,flightId]{
    const auto prior=detail::snapshots(captured);
    Json backups=Json::object();
    if(!prior.empty())for(const auto& path:detail::tracked(captured)){
      try{
        const auto file=self->expandPath(path);
        const auto& old=value::property(value::property(prior.back(),
          "trackedFileBackups"),path);
        const auto version=old.is_object()?detail::version(old)+1:1;
        std::error_code error;
        const bool exists=std::filesystem::exists(file,error);
        if(error)throw std::filesystem::filesystem_error(
          "stat tracked file",file,error);
        if(!exists){
          backups[path]={{"backupFileName",nullptr},{"version",version},
            {"backupTime",platform::isoTimestamp()}};
          continue;
        }
        const auto name=detail::stringField(old,"backupFileName");
        if(!name.empty()&&!self->changed(file,name,flightId))backups[path]=old;
        else backups[path]=self->createBackup(file,version,flightId);
      }catch(...){self->report(std::current_exception());}
    }
    return backups;
  });
  worker.then([self,messageId,flightId,result](const Json* generated,
      const std::exception_ptr& error){
    if(error){self->report(error);result.reject(error);return;}
    if(self->inputs_.flight->id()!=flightId){result.resolve({});return;}
    try{
      Json backups=*generated;
      auto current=self->state();
      auto entries=detail::snapshots(current);
      if(!entries.empty())for(const auto& path:detail::tracked(current)){
        if(backups.contains(path))continue;
        const auto& inherited=value::property(value::property(entries.back(),
          "trackedFileBackups"),path);
        if(inherited.is_object())backups[path]=inherited;
      }
      Json snapshot={{"messageId",messageId},
        {"trackedFileBackups",std::move(backups)},
        {"timestamp",platform::isoTimestamp()}};
      const Json old=entries.empty()?Json():entries.back();
      entries.push_back(snapshot);
      if(entries.size()>100)entries.erase(entries.begin(),
        entries.begin()+static_cast<std::ptrdiff_t>(entries.size()-100));
      current["snapshots"]=std::move(entries);
      const auto& sequence=value::property(current,"snapshotSequence");
      current["snapshotSequence"]=(sequence.is_number_integer()?
        sequence.get<std::int64_t>():0)+1;
      self->setState(current);
      self->record(messageId,snapshot,false);
      try{notifyChanges(*self,self->inputs_,old,snapshot,current);}
      catch(...){self->report(std::current_exception());}
      result.resolve({});
    }catch(...){self->report(std::current_exception());
      result.reject(std::current_exception());}
  });
  return result;
}
} // namespace batbox::application_file_history
