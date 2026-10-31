#include "application_hooks/FileChangedWatcher.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown FileChanged hook error";}
  return {};
}
event_loop::Deferred<event_loop::Unit> ready(event_loop::Loop& owner){
  event_loop::Deferred<event_loop::Unit> value(owner);value.resolve({});return value;
}
Json eventHooks(const Json& config,const char* event){
  const auto& root=value::property(config,"hooks");
  return value::property(root.is_object()?root:config,event);
}
std::vector<std::string> matcherNames(const Json& config){
  std::vector<std::string> names;
  const auto matchers=eventHooks(config,"FileChanged");
  if(!matchers.is_array())return names;
  for(const auto& matcher:matchers){
    const auto& value=value::property(matcher,"matcher");
    if(!value.is_string())continue;
    const auto text=value.get<std::string>();
    std::size_t from=0;
    while(from<=text.size()){
      const auto end=text.find('|',from);
      const auto name=value::trim(text.substr(from,end==std::string::npos?
        std::string::npos:end-from));
      if(!name.empty())names.push_back(name);
      if(end==std::string::npos)break;
      from=end+1;
    }
  }
  return names;
}
bool hasEnvHooks(const Json& config){
  const auto cwd=eventHooks(config,"CwdChanged");
  const auto file=eventHooks(config,"FileChanged");
  return (cwd.is_array()&&!cwd.empty())||
    (file.is_array()&&!file.empty());
}
}
std::shared_ptr<FileChangedWatcher> FileChangedWatcher::create(
    FileChangedInputs inputs){
  return std::make_shared<FileChangedWatcher>(std::move(inputs));
}
FileChangedWatcher::FileChangedWatcher(FileChangedInputs inputs):
    inputs_(std::move(inputs)){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "FileChanged watcher requires owner loop");
  if(inputs_.cwd.empty()||!inputs_.hooksConfig||!inputs_.fileChanged||
      !inputs_.cwdChanged||!inputs_.clearCwdEnvFiles)
    throw std::invalid_argument("FileChanged watcher services required");
  if(!inputs_.log)inputs_.log=[](const std::string&){};
  if(!inputs_.notify)inputs_.notify=[](const std::string&,bool){};
}
FileChangedWatcher::~FileChangedWatcher(){close();}
std::vector<std::string> FileChangedWatcher::resolveWatchPaths()const{
  const auto cwd=runtime::pathFromUtf8(inputs_.cwd);
  std::set<std::string> paths;
  for(const auto& name:matcherNames(inputs_.hooksConfig())){
    const auto path=runtime::pathFromUtf8(name);
    paths.insert(runtime::pathToUtf8((path.is_absolute()?path:cwd/path)
      .lexically_normal()));
  }
  for(const auto& name:dynamicWatchPaths_){
    const auto path=runtime::pathFromUtf8(name);
    paths.insert(runtime::pathToUtf8((path.is_absolute()?path:cwd/path)
      .lexically_normal()));
  }
  return {paths.begin(),paths.end()};
}
void FileChangedWatcher::restart(){
  if(watcher_){watcher_->close();watcher_.reset();}
  if(closed_||!initialized_)return;
  const auto paths=resolveWatchPaths();
  if(paths.empty())return;
  settings::persistence::WatchTargets targets;
  targets.settingsFiles=paths;
  targets.includeGitPaths=true;
  std::set<std::string> dirs;
  for(const auto& path:paths){
    const auto dir=runtime::pathToUtf8(
      runtime::pathFromUtf8(path).parent_path());
    if(!dir.empty())dirs.insert(dir);
  }
  targets.dirs.assign(dirs.begin(),dirs.end());
  inputs_.log("FileChanged: watching "+std::to_string(paths.size())+" paths");
  const auto weak=weak_from_this();
  settings::watcher::Events events;
  events.change=[weak](const std::string& path){
    if(auto self=weak.lock())self->fileEvent(path,"change");};
  events.add=[weak](const std::string& path){
    if(auto self=weak.lock())self->fileEvent(path,"add");};
  events.unlink=[weak](const std::string& path){
    if(auto self=weak.lock())self->fileEvent(path,"unlink");};
  events.error=[weak](const std::string& message){
    if(auto self=weak.lock())self->inputs_.log("FileChanged watcher: "+message);};
  watcher_=std::make_unique<settings::watcher::IndigenousWatcher>(
    std::move(targets),inputs_.owner.dispatcher(),std::move(events),
    settings::watcher::Options{500,200,100});
}
void FileChangedWatcher::start(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "FileChanged watcher requires owner loop");
  if(initialized_||closed_)return;
  initialized_=true;
  restart();
}
void FileChangedWatcher::updateWatchPaths(std::vector<std::string> paths){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "FileChanged watcher requires owner loop");
  if(!initialized_||closed_)return;
  auto sorted=paths;std::sort(sorted.begin(),sorted.end());
  if(sorted==dynamicWatchPathsSorted_)return;
  dynamicWatchPaths_=std::move(paths);
  dynamicWatchPathsSorted_=std::move(sorted);
  restart();
}
void FileChangedWatcher::report(const hooks::EnvHookResult& result){
  if(!result.watchPaths.empty())updateWatchPaths(result.watchPaths);
  for(const auto& message:result.systemMessages)
    inputs_.notify(message,false);
  for(const auto& row:result.results)
    if(!row.succeeded&&!row.output.empty())inputs_.notify(row.output,true);
}
void FileChangedWatcher::fileEvent(const std::string& path,
    const std::string& event){
  if(closed_)return;
  inputs_.log("FileChanged: "+event+" "+path);
  try{
    const auto self=shared_from_this();
    inputs_.fileChanged(path,event).then([self](
      const hooks::EnvHookResult* result,const std::exception_ptr& error){
      if(self->closed_)return;
      if(error){const auto text=errorText(error);
        self->inputs_.log("FileChanged hook failed: "+text);
        self->inputs_.notify(text,true);return;}
      self->report(*result);
    });
  }catch(...){const auto text=errorText(std::current_exception());
    inputs_.log("FileChanged hook failed: "+text);inputs_.notify(text,true);}
}
event_loop::Deferred<event_loop::Unit> FileChangedWatcher::onCwdChanged(
    const std::string& oldCwd,const std::string& newCwd){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "FileChanged watcher requires owner loop");
  if(closed_||oldCwd==newCwd)return ready(inputs_.owner);
  event_loop::Deferred<event_loop::Unit> output(inputs_.owner);
  try{
    if(!hasEnvHooks(inputs_.hooksConfig())){output.resolve({});return output;}
    inputs_.cwd=newCwd;
    const auto self=shared_from_this();
    inputs_.clearCwdEnvFiles().then([self,oldCwd,newCwd,output](
      const event_loop::Unit*,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      if(self->closed_){output.resolve({});return;}
      self->inputs_.cwdChanged(oldCwd,newCwd).then(
        [self,output](const hooks::EnvHookResult* result,
          const std::exception_ptr& error){
          if(self->closed_){output.resolve({});return;}
          if(error){const auto text=errorText(error);
            self->inputs_.log("CwdChanged hook failed: "+text);
            self->inputs_.notify(text,true);
            self->dynamicWatchPaths_.clear();
            self->dynamicWatchPathsSorted_.clear();
          }else{
            self->dynamicWatchPaths_=result->watchPaths;
            self->dynamicWatchPathsSorted_=result->watchPaths;
            std::sort(self->dynamicWatchPathsSorted_.begin(),
              self->dynamicWatchPathsSorted_.end());
            for(const auto& message:result->systemMessages)
              self->inputs_.notify(message,false);
            for(const auto& row:result->results)
              if(!row.succeeded&&!row.output.empty())
                self->inputs_.notify(row.output,true);
          }
          self->restart();output.resolve({});
        });
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void FileChangedWatcher::close(){
  closed_=true;
  if(watcher_){watcher_->close();watcher_.reset();}
  dynamicWatchPaths_.clear();dynamicWatchPathsSorted_.clear();
  initialized_=false;
}
} // namespace batbox::application_hooks
