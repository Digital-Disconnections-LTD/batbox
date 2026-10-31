#include "settings/persistence/ChangeEvents.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::settings::persistence {
ChangeEvents::ChangeEvents(ChangeEventServices services):services_(std::move(services)){
  if(!services_.internalWrites||!services_.debug||
      !services_.executeConfigChangeHooks||!services_.setTimeout||
      !services_.resetSettingsStash)
    throw std::invalid_argument("Missing settings change event binding");
}
std::function<void()> ChangeEvents::subscribe(Listener listener){
  auto entry=std::make_shared<ListenerEntry>(std::move(listener));
  listeners_.push_back(entry);
  return [entry](){entry->active=false;};
}
void ChangeEvents::cancelDeletion(const std::string& path,
    const std::string& reason){
  const auto found=pendingDeletions_.find(path);
  if(found==pendingDeletions_.end())return;
  found->second();
  pendingDeletions_.erase(found);
  services_.debug("Cancelled pending deletion of "+path+
    " — file was "+reason);
}
void ChangeEvents::runHooks(Source source,const std::string& path,
    bool deletion){
  services_.executeConfigChangeHooks(
    settingSourceToConfigChangeSource(source),path,
    [this,source,path,deletion](const std::vector<bool>& results){
      if(std::any_of(results.begin(),results.end(),
          [](bool blocked){return blocked;})){
        services_.debug("ConfigChange hook blocked "+
          std::string(deletion?"deletion of ":"change to ")+path);
        return;
      }
      fanOut(source);
    });
}
void ChangeEvents::handleChange(const std::string& path){
  const auto source=getSourceForPath(services_.paths,path);
  if(!source)return;
  cancelDeletion(path,"recreated");
  if(services_.internalWrites->consumeInternalWrite(path,5000))return;
  services_.debug("Detected change to "+path);
  runHooks(*source,path,false);
}
void ChangeEvents::handleAdd(const std::string& path){
  const auto source=getSourceForPath(services_.paths,path);
  if(!source)return;
  cancelDeletion(path,"re-added");
  handleChange(path);
}
void ChangeEvents::handleDelete(const std::string& path){
  const auto source=getSourceForPath(services_.paths,path);
  if(!source)return;
  services_.debug("Detected deletion of "+path);
  if(pendingDeletions_.contains(path))return;
  auto cancel=services_.setTimeout(services_.deletionGraceMs,
    [this,path,source=*source](){
      pendingDeletions_.erase(path);
      runHooks(source,path,true);
    });
  pendingDeletions_[path]=std::move(cancel);
}
void ChangeEvents::fanOut(Source source){
  services_.resetSettingsStash();
  for(std::size_t i=0;i<listeners_.size();++i)
    if(listeners_[i]->active)listeners_[i]->callback(source);
}
void ChangeEvents::notifyChange(Source source){
  services_.debug("Programmatic settings change notification for "+
    sourceKey(source));
  fanOut(source);
}
void ChangeEvents::setDeletionGrace(double milliseconds){
  services_.deletionGraceMs=milliseconds;
}
void ChangeEvents::clearPendingDeletions(){
  for(auto& [path,cancel]:pendingDeletions_){(void)path;cancel();}
  pendingDeletions_.clear();
}
void ChangeEvents::clearListeners(){
  for(auto& entry:listeners_)entry->active=false;
  listeners_.clear();
}
void ChangeEvents::dispose(){
  clearPendingDeletions();
  services_.internalWrites->clearInternalWrites();
  clearListeners();
}
}
