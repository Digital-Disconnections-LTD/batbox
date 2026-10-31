#include "instructions/hooks/runtime/FlightEnvironmentInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>

namespace batbox::instructions::hooks::runtime {
namespace {
struct LoadRun:std::enable_shared_from_this<LoadRun>{
  std::shared_ptr<FlightEnvironment::State> state;
  event_loop::Deferred<std::optional<std::string>> output;
  std::vector<std::string> scripts,hookFiles;
  std::string directory;
  std::size_t index=0;
  explicit LoadRun(std::shared_ptr<FlightEnvironment::State> owner):
    state(std::move(owner)),output(state->loop){}
  void reject(std::exception_ptr error){output.reject(error);}
  void logFailure(const std::string& prefix,std::exception_ptr error){
    if(state->imports.errnoCode(error)!=std::optional<std::string>("ENOENT"))
      state->imports.logDebug(prefix+state->imports.errorMessage(error));
  }
  void start(){
    try{
      if(state->imports.getPlatform()=="windows"){
        state->imports.logDebug("Session environment not yet supported on Windows");
        output.resolve(std::nullopt);return;
      }
      if(state->script){output.resolve(*state->script);return;}
      const auto parentFile=state->imports.getParentEnvFile();
      if(parentFile&&!parentFile->empty())readParent(*parentFile);
      else loadDirectory();
    }catch(...){reject(std::current_exception());}
  }
  void readParent(const std::string& path){
    try{
      state->imports.readFile(path).then(
        [self=shared_from_this(),path](const std::string* content,
          const std::exception_ptr& error){
          try{
            if(error)self->logFailure("Failed to read BATBOX_ENV_FILE: ",error);
            else{
              const auto script=value::trim(*content);
              if(!script.empty()){
                self->scripts.push_back(script);
                self->state->imports.logDebug("Session environment loaded from "
                  "BATBOX_ENV_FILE: "+path+" ("+
                  std::to_string(value::stringLength(script))+" chars)");
              }
            }
            self->loadDirectory();
          }catch(...){self->reject(std::current_exception());}
        });
    }catch(...){
      try{logFailure("Failed to read BATBOX_ENV_FILE: ",
        std::current_exception());loadDirectory();}
      catch(...){reject(std::current_exception());}
    }
  }
  void loadDirectory(){
    try{
      const auto home=state->imports.getConfigHomeDir();
      const auto flight=state->imports.getFlightId();
      directory=joinFlightPath(joinFlightPath(home,"session-env"),flight);
      state->imports.mkdir(directory).then(
        [self=shared_from_this()](const event_loop::Unit*,
          const std::exception_ptr& error){
          if(error){self->reject(error);return;}
          self->listFiles();
        });
    }catch(...){reject(std::current_exception());}
  }
  void listFiles(){
    try{
      state->imports.readdir(directory).then(
        [self=shared_from_this()](const std::vector<std::string>* files,
          const std::exception_ptr& error){
          try{
            if(error){
              self->logFailure("Failed to load session environment from hooks: ",
                error);
              self->complete();return;
            }
            for(const auto& file:*files)
              if(isHookEnvFile(file))self->hookFiles.push_back(file);
            sortHookEnvFiles(self->hookFiles);
            self->readNext();
          }catch(...){self->reject(std::current_exception());}
        });
    }catch(...){
      try{logFailure("Failed to load session environment from hooks: ",
        std::current_exception());complete();}
      catch(...){reject(std::current_exception());}
    }
  }
  void readNext(){
    if(index==hookFiles.size()){
      try{
        if(!hookFiles.empty())state->imports.logDebug(
          "Session environment loaded from "+std::to_string(hookFiles.size())+
          " hook file(s)");
        complete();
      }catch(...){reject(std::current_exception());}
      return;
    }
    const auto path=joinFlightPath(directory,hookFiles[index++]);
    try{
      state->imports.readFile(path).then(
        [self=shared_from_this(),path](const std::string* content,
          const std::exception_ptr& error){
          try{
            if(error)self->logFailure("Failed to read hook file "+path+": ",error);
            else{
              const auto script=value::trim(*content);
              if(!script.empty())self->scripts.push_back(script);
            }
            self->readNext();
          }catch(...){self->reject(std::current_exception());}
        });
    }catch(...){
      try{logFailure("Failed to read hook file "+path+": ",
        std::current_exception());readNext();}
      catch(...){reject(std::current_exception());}
    }
  }
  void complete(){
    try{
      if(scripts.empty()){
        state->imports.logDebug("No session environment scripts found");
        state->script=std::optional<std::string>{};
        output.resolve(std::nullopt);return;
      }
      std::string joined;
      for(const auto& script:scripts){
        if(!joined.empty())joined+='\n';
        joined+=script;
      }
      state->script=joined;
      state->imports.logDebug("Session environment script ready ("+
        std::to_string(value::stringLength(joined))+" chars total)");
      output.resolve(joined);
    }catch(...){reject(std::current_exception());}
  }
};
}
event_loop::Deferred<std::optional<std::string>>
FlightEnvironment::getFlightEnvironmentScript()const{
  auto run=std::make_shared<LoadRun>(state_);
  run->start();return run->output;
}
} // namespace batbox::instructions::hooks::runtime
