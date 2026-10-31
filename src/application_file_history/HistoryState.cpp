#include "application_file_history/HistoryInternal.hpp"
#include "application/Async.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace batbox::application_file_history {
namespace detail {
Json emptyState(){return {{"snapshots",Json::array()},
  {"trackedFiles",Json::object()},{"snapshotSequence",0}};}
std::vector<std::string> tracked(const Json& state){
  std::vector<std::string> result;
  const auto& files=value::property(state,"trackedFiles");
  if(files.is_object())for(auto it=files.begin();it!=files.end();++it)
    result.push_back(it.key());
  else if(files.is_array())for(const auto& value:files)
    if(value.is_string())result.push_back(value.get<std::string>());
  return result;
}
Json snapshots(const Json& state){
  const auto& value=value::property(state,"snapshots");
  return value.is_array()?value:Json::array();
}
std::string stringField(const Json& value,std::string_view key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():"";
}
std::size_t version(const Json& backup){
  const auto& value=value::property(backup,"version");
  return value.is_number_unsigned()?value.get<std::size_t>():
    value.is_number_integer()&&value.get<std::int64_t>()>0?
      static_cast<std::size_t>(value.get<std::int64_t>()):0;
}
bool envTruthy(const char* name){
  const char* value=std::getenv(name);
  if(!value||!*value)return false;
  const std::string text=value;
  return text!="0"&&text!="false"&&text!="FALSE"&&text!="no";
}
std::optional<std::string> readOrNull(const std::filesystem::path& file){
  std::ifstream stream(file,std::ios::binary);
  if(!stream)return {};
  return std::string(std::istreambuf_iterator<char>(stream),{});
}
}
std::shared_ptr<History> History::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread())
    throw std::logic_error("File history requires owner loop");
  if(!inputs.store||!inputs.flight||inputs.configHome.empty()||
      inputs.originalCwd.empty())
    throw std::invalid_argument("File history requires store, session and paths");
  return std::shared_ptr<History>(new History(std::move(inputs)));
}
bool History::enabled()const{
  if(detail::envTruthy("CLAUDE_CODE_DISABLE_FILE_CHECKPOINTING"))return false;
  if(inputs_.nonInteractive)
    return detail::envTruthy("CLAUDE_CODE_ENABLE_SDK_FILE_CHECKPOINTING");
  return !inputs_.configuredEnabled||inputs_.configuredEnabled();
}
Json History::state()const{
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("File history state requires owner loop");
  const Json snapshot=inputs_.store->getState()->snapshot();
  const auto& value=value::property(snapshot,"fileHistory");
  return value.is_object()?value:detail::emptyState();
}
void History::setState(Json value){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("File history update requires owner loop");
  inputs_.store->setState([value=std::move(value)](
      const app_state::AppStateRef& previous){
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->fields->set("fileHistory",value);
    return next;
  });
}
void History::report(std::exception_ptr error)const{
  if(!inputs_.logError)return;
  if(inputs_.owner.isOwnerThread()){
    inputs_.logError(error);return;
  }
  const auto dispatch=inputs_.owner.dispatcher();
  try{dispatch.postMission([callback=inputs_.logError,error]{callback(error);});}
  catch(...){/* The owner has already shut down. */}
}
void History::record(const std::string& id,const Json& snapshot,bool update){
  auto pending=inputs_.flight->project()->insertFileHistorySnapshot(
    id,snapshot,update);
  auto weak=weak_from_this();
  pending.then([weak](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)if(auto self=weak.lock())self->report(error);
  });
}
std::string History::trackingPath(const std::filesystem::path& file)const{
  const auto value=runtime::pathToUtf8(file);
  if(!file.is_absolute())return value;
  const auto cwd=runtime::pathToUtf8(inputs_.originalCwd);
  if(value.starts_with(cwd))return runtime::pathToUtf8(
    file.lexically_relative(inputs_.originalCwd));
  return value;
}
std::filesystem::path History::expandPath(const std::string& file)const{
  const auto path=runtime::pathFromUtf8(file);
  return path.is_absolute()?path:inputs_.originalCwd/path;
}
std::filesystem::path History::backupPath(const std::string& name,
    std::optional<std::string> flightId)const{
  return inputs_.configHome/"file-history"/
    (flightId?*flightId:inputs_.flight->id())/name;
}
std::optional<Json> History::findSnapshot(const Json& state,
    const std::string& id)const{
  const auto entries=detail::snapshots(state);
  for(auto it=entries.rbegin();it!=entries.rend();++it)
    if(detail::stringField(*it,"messageId")==id)return *it;
  return {};
}
bool History::canRestore(const std::string& id)const{
  return enabled()&&findSnapshot(state(),id).has_value();
}
std::optional<std::optional<std::string>> History::backupAt(
    const Json& state,const Json& snapshot,const std::string& path)const{
  const auto& backups=value::property(snapshot,"trackedFileBackups");
  const auto& backup=value::property(backups,path);
  if(backup.is_object()){
    const auto& name=value::property(backup,"backupFileName");
    if(name.is_null())return std::optional<std::string>{};
    if(name.is_string())return name.get<std::string>();
  }
  for(const auto& older:detail::snapshots(state)){
    const auto& prior=value::property(value::property(older,"trackedFileBackups"),path);
    if(!prior.is_object()||detail::version(prior)!=1)continue;
    const auto& name=value::property(prior,"backupFileName");
    if(name.is_null())return std::optional<std::string>{};
    if(name.is_string())return name.get<std::string>();
  }
  return {};
}
void History::restoreStateFromLog(const Json& snapshots){
  if(!enabled()||!snapshots.is_array())return;
  Json migrated=Json::array(),tracked=Json::object();
  for(const auto& snapshot:snapshots){
    if(!snapshot.is_object())continue;
    Json next=snapshot,backups=Json::object();
    const auto& source=value::property(snapshot,"trackedFileBackups");
    if(source.is_object())for(auto it=source.begin();it!=source.end();++it){
      const auto key=trackingPath(runtime::pathFromUtf8(it.key()));
      backups[key]=it.value();tracked[key]=true;
    }
    next["trackedFileBackups"]=std::move(backups);
    migrated.push_back(std::move(next));
  }
  setState({{"snapshots",migrated},{"trackedFiles",tracked},
    {"snapshotSequence",migrated.size()}});
}
} // namespace batbox::application_file_history
