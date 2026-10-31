#include "attachments/local/LocalCollectors.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "query/OwnerPromiseAll.hpp"
#include <memory>
#include <stdexcept>
#include <system_error>

namespace batbox::attachments::local {
namespace {
bool indigenousEnoent(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const filesystem::RangeError& failure){
    return failure.code==std::errc::no_such_file_or_directory;
  }catch(const std::system_error& failure){
    return failure.code()==std::errc::no_such_file_or_directory;
  }catch(...){return false;}
}
struct FileRun:std::enable_shared_from_this<FileRun> {
  std::string key,normalized;
  file_state_stash::FileStateStash::Value state;
  execution::Context& context;
  app_state::AppStateRef app;
  Services services;
  event_loop::Deferred<std::optional<Json>> output;
  FileRun(std::string path,execution::Context& turn,
      app_state::AppStateRef current,Services imports):key(std::move(path)),
      context(turn),app(std::move(current)),services(std::move(imports)),
      output(services.loop){}
  void finish(){output.resolve(std::nullopt);}
  void fail(const std::exception_ptr& error){
    try{
      if(services.isENOENT?services.isENOENT(error):indigenousEnoent(error))
        context.readFileState->remove(key);
      finish();
    }catch(...){output.reject(std::current_exception());}
  }
  void start(){
    try{
      state=context.readFileState->get(key);
      if(!state||state->offset||state->limit){finish();return;}
      if(!services.files||!services.files->expandPath||
         !services.files->isFileReadDenied)
        throw std::invalid_argument("Changed-file path and permission imports required");
      normalized=services.files->expandPath(key);
      if(services.files->isFileReadDenied(normalized,app)){
        finish();return;
      }
      if(!services.files->modificationTime)
        throw std::invalid_argument("Changed-file mtime import required");
      services.files->modificationTime(normalized).then(
        [self=shared_from_this()](const double* mtime,
            const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          if(!mtime||*mtime<=self->state->timestamp){self->finish();return;}
          self->validate();
        });
    }catch(...){output.reject(std::current_exception());}
  }
  void validate(){
    try{
      if(!services.files->validateRead)
        throw std::invalid_argument("Changed-file Read validator import required");
      const Json request={{"file_path",normalized}};
      services.files->validateRead(request,context).then(
        [self=shared_from_this(),request](const gadgets::ValidationResult* result,
            const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          if(!result||!result->result){self->finish();return;}
          self->read(request);
        });
    }catch(...){fail(std::current_exception());}
  }
  void read(const Json& request){
    try{
      if(!services.files->callRead)
        throw std::invalid_argument("Changed-file Read import required");
      services.files->callRead(request,context).then(
        [self=shared_from_this()](const gadgets::GadgetResult* result,
            const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          try{
            if(!result){self->finish();return;}
            const auto& data=result->data;
            const auto& type=value::property(data,"type");
            if(type=="text"){
              const auto content=value::property(value::property(data,"file"),
                "content").get<std::string>();
              const auto snippet=getSnippetForTwoFileDiff(
                self->state->content,content,self->services);
              if(snippet.empty()){self->finish();return;}
              self->output.resolve(Json{{"type","edited_text_file"},
                {"filename",self->normalized},{"snippet",snippet}});
            }else if(type=="image")self->image();
            else self->finish();
          }catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }
  void image(){
    try{
      if(!services.readImageWithTokenBudget)
        throw std::invalid_argument("Changed-image read import required");
      services.readImageWithTokenBudget(normalized).then(
        [self=shared_from_this()](const Json* image,
            const std::exception_ptr& error){
          if(error){
            try{
              if(!self->services.logError)
                throw std::invalid_argument("Changed-image error logger required");
              self->services.logError(error);
              self->finish();
            }catch(...){self->output.reject(std::current_exception());}
            return;
          }
          if(!image){self->finish();return;}
          self->output.resolve(Json{{"type","edited_image_file"},
            {"filename",self->normalized},{"content",*image}});
        });
    }catch(...){
      // Image compression has its own error path and never evicts the stash.
      if(!services.logError){output.reject(std::current_exception());return;}
      services.logError(std::current_exception());finish();
    }
  }
};
}
event_loop::Deferred<Json> getChangedFiles(execution::Context& context,
    const Services& services){
  event_loop::Deferred<Json> output(services.loop);
  try{
    if(!context.readFileState)
      throw std::invalid_argument("Typed file-state cache required");
    const auto keys=file_state_stash::stashKeys(*context.readFileState);
    if(keys.empty()){output.resolve(Json::array());return output;}
    if(!services.getAppState)
      throw std::invalid_argument("Live AppState import required");
    const auto app=services.getAppState();
    std::vector<event_loop::Deferred<std::optional<Json>>> pending;
    pending.reserve(keys.size());
    for(const auto& key:keys){
      auto run=std::make_shared<FileRun>(key,context,app,services);
      pending.push_back(run->output);
      run->start();
    }
    query::ownerPromiseAll(services.loop,pending).then(
      [output](const std::vector<std::optional<Json>>* values,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        Json attachments=Json::array();
        if(values)for(const auto& value:*values)
          if(value)attachments.push_back(*value);
        output.resolve(std::move(attachments));
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::attachments::local
