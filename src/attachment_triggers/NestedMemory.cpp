#include "attachment_triggers/Attachments.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::attachment_triggers {
namespace {
using event_loop::Deferred;
void append(Json& to,const Json& from){
  for(const auto& attachment:from)to.push_back(attachment);
}
MemoryFiles filterProjectLevel(const MemoryFiles& files,bool skip){
  if(!skip)return files;
  MemoryFiles filtered;
  for(const auto& file:files)
    if(file.type!="Project"&&file.type!="Local")filtered.push_back(file);
  return filtered;
}

struct FileRun:std::enable_shared_from_this<FileRun> {
  std::string filePath;
  execution::Context& context;
  app_state::AppStateRef appState;
  Services services;
  Deferred<Json> output;
  Json attachments=Json::array();
  std::shared_ptr<PathSet> processed=std::make_shared<PathSet>();
  DirectoriesToProcess dirs;
  bool skipProjectLevel=false;
  FileRun(std::string path,execution::Context& turn,app_state::AppStateRef app,
    Services imports):filePath(std::move(path)),context(turn),
    appState(std::move(app)),services(std::move(imports)),output(services.loop){}
  void failed(const std::exception_ptr& error){
    try{
      if(!services.logError)throw std::invalid_argument(
        "Nested memory error logger required");
      services.logError(error);
      output.resolve(std::move(attachments));
    }catch(...){output.reject(std::current_exception());}
  }
  void add(const MemoryFiles& files){
    append(attachments,memoryFilesToAttachments(files,context,services,filePath));
  }
  void start(){
    try{
      if(!services.pathInAllowedWorkingPath)
        throw std::invalid_argument("Allowed working path import required");
      if(!services.pathInAllowedWorkingPath(filePath,appState)){
        output.resolve(std::move(attachments));return;
      }
      instructions::project::async::getManagedAndUserConditionalRules(
        services.loop,filePath,processed,services.memory).then(
        [self=shared_from_this()](const MemoryFiles* files,
          const std::exception_ptr& error){
          if(error){self->failed(error);return;}
          try{self->add(*files);self->afterManaged();}
          catch(...){self->failed(std::current_exception());}
        });
    }catch(...){failed(std::current_exception());}
  }
  void afterManaged(){
    if(!services.memory.sync.originalCwd)
      throw std::invalid_argument("Original instruction CWD import required");
    dirs=getDirectoriesToProcess(filePath,services.memory.sync.originalCwd(),
      services.cwd);
    if(!services.skipProjectLevel)
      throw std::invalid_argument("Project instruction feature import required");
    skipProjectLevel=services.skipProjectLevel();
    nested(0);
  }
  void nested(std::size_t index){
    if(index==dirs.nestedDirs.size()){cwdLevel(0);return;}
    try{
      instructions::project::async::getMemoryFilesForNestedDirectory(
        services.loop,dirs.nestedDirs[index],filePath,processed,services.memory)
        .then([self=shared_from_this(),index](const MemoryFiles* files,
          const std::exception_ptr& error){
          if(error){self->failed(error);return;}
          try{
            self->add(filterProjectLevel(*files,self->skipProjectLevel));
            self->nested(index+1);
          }catch(...){self->failed(std::current_exception());}
        });
    }catch(...){failed(std::current_exception());}
  }
  void cwdLevel(std::size_t index){
    if(index==dirs.cwdLevelDirs.size()){
      output.resolve(std::move(attachments));return;
    }
    try{
      instructions::project::async::getConditionalRulesForCwdLevelDirectory(
        services.loop,dirs.cwdLevelDirs[index],filePath,processed,services.memory)
        .then([self=shared_from_this(),index](const MemoryFiles* files,
          const std::exception_ptr& error){
          if(error){self->failed(error);return;}
          try{
            self->add(filterProjectLevel(*files,self->skipProjectLevel));
            self->cwdLevel(index+1);
          }catch(...){self->failed(std::current_exception());}
        });
    }catch(...){failed(std::current_exception());}
  }
};

struct TriggerRun:std::enable_shared_from_this<TriggerRun> {
  execution::Context& context;
  Services services;
  app_state::AppStateRef appState;
  std::shared_ptr<runtime::OrderedStringSet> triggers;
  runtime::OrderedStringSet::const_iterator current;
  Deferred<Json> output;
  Json attachments=Json::array();
  TriggerRun(execution::Context& turn,Services imports,
    app_state::AppStateRef app):context(turn),services(std::move(imports)),
    appState(std::move(app)),triggers(context.nestedMemoryAttachmentTriggers),
    current(triggers->begin()),output(services.loop){}
  void next(){
    if(current==triggers->end()){
      triggers->clear();output.resolve(std::move(attachments));return;
    }
    const auto path=*current;
    try{
      getNestedMemoryAttachmentsForFile(path,context,appState,services).then(
        [self=shared_from_this()](const Json* value,
          const std::exception_ptr& error){
          if(error){self->output.reject(error);return;}
          try{
            if(value)append(self->attachments,*value);
            ++self->current;
            self->next();
          }catch(...){self->output.reject(std::current_exception());}
        });
    }catch(...){output.reject(std::current_exception());}
  }
};
}

Deferred<Json> getNestedMemoryAttachmentsForFile(const std::string& filePath,
  execution::Context& context,const app_state::AppStateRef& appState,
  const Services& services){
  if(!services.loop.isOwnerThread())
    throw std::logic_error("Nested memory requires owner event loop");
  auto run=std::make_shared<FileRun>(filePath,context,appState,services);
  run->start();
  return run->output;
}

Deferred<Json> getNestedMemoryAttachments(execution::Context& context,
  const Services& services){
  if(!services.loop.isOwnerThread())
    throw std::logic_error("Nested memory requires owner event loop");
  Deferred<Json> empty(services.loop);
  if(!context.nestedMemoryAttachmentTriggers||
     context.nestedMemoryAttachmentTriggers->empty()){
    empty.resolve(Json::array());return empty;
  }
  if(!services.getAppState)
    {empty.reject(std::make_exception_ptr(std::invalid_argument(
      "Live AppState import required")));return empty;}
  try{
    auto run=std::make_shared<TriggerRun>(context,services,
      services.getAppState());
    run->next();return run->output;
  }catch(...){empty.reject(std::current_exception());return empty;}
}
} // namespace batbox::attachment_triggers
