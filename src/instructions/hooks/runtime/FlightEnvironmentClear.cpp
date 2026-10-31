#include "instructions/hooks/runtime/FlightEnvironmentInternal.hpp"
#include <memory>

namespace batbox::instructions::hooks::runtime {
namespace {
struct ClearRun:std::enable_shared_from_this<ClearRun>{
  std::shared_ptr<FlightEnvironment::State> state;
  event_loop::Deferred<event_loop::Unit> output;
  std::string directory;
  std::size_t pending=0;
  bool finished=false;
  explicit ClearRun(std::shared_ptr<FlightEnvironment::State> owner):
    state(std::move(owner)),output(state->loop){}
  void fail(std::exception_ptr error){
    if(finished)return;
    finished=true;
    try{
      if(state->imports.errnoCode(error)!=std::optional<std::string>("ENOENT"))
        state->imports.logDebug("Failed to clear cwd env files: "+
          state->imports.errorMessage(error));
      output.resolve({});
    }catch(...){output.reject(std::current_exception());}
  }
  void afterDirectory(const std::string& value){
    directory=value;
    try{
      state->imports.readdir(directory).then(
        [self=shared_from_this()](const std::vector<std::string>* files,
          const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          self->afterListing(*files);
        });
    }catch(...){fail(std::current_exception());}
  }
  void afterListing(const std::vector<std::string>& files){
    std::vector<event_loop::Deferred<event_loop::Unit>> writes;
    try{
      for(const auto& file:files)
        if(isCwdEnvFile(file))writes.push_back(state->imports.writeFile(
          joinFlightPath(directory,file),""));
    }catch(...){fail(std::current_exception());return;}
    pending=writes.size();
    if(pending==0){finished=true;output.resolve({});return;}
    for(auto& write:writes)write.then(
      [self=shared_from_this()](const event_loop::Unit*,
        const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        if(--self->pending==0&&!self->finished){
          self->finished=true;self->output.resolve({});
        }
      });
  }
};
}
event_loop::Deferred<event_loop::Unit> FlightEnvironment::clearCwdEnvFiles()const{
  auto run=std::make_shared<ClearRun>(state_);
  try{
    getFlightEnvDirPath().then([run](const std::string* path,
      const std::exception_ptr& error){
      if(error)run->fail(error);else run->afterDirectory(*path);
    });
  }catch(...){run->fail(std::current_exception());}
  return run->output;
}
} // namespace batbox::instructions::hooks::runtime
