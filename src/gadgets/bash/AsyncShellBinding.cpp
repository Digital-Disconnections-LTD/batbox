#include "runtime/AsyncLaunch.hpp"
#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "shell/AsyncCommandResult.hpp"
#include <memory>
#include <thread>
#include <utility>

namespace batbox::bash_gadget {
AsyncShellCommand bindAsyncShellCommand(event_loop::Loop& loop,shell::CommandHandle handle){
  AsyncShellCommand bound(shell::resultOnLoop(loop,handle));
  bound.missionId=handle.missionId();bound.outputPath=handle.outputPath();
  bound.status=[handle]{return handle.status();};
  bound.cleanup=[handle]{handle.cleanup();};
  bound.background=[handle](const std::string& id){return handle.background(id);};
  bound.largeOutputInfo=[handle]{return handle.largeOutputInfo();};
  const auto dispatcher=loop.dispatcher();
  const auto weakBackground=handle.weakBackgroundCallback();
  bound.onTimeout=[handle,dispatcher,weakBackground](
    const shell::CommandHandle::TimeoutCallback& callback){
    handle.onTimeout([dispatcher,callback,weakBackground](const auto&){
      // The shell timeout is raised on its wait worker. Dispatch its callback
      // to the owner using a weak command operation, avoiding both the worker's
      // stack callback and a handle-State cycle.
      dispatcher.postMission([callback,weakBackground]{
        callback(weakBackground);
      });
    });
  };
  return bound;
}
event_loop::Deferred<AsyncShellCommand> execManagedOnLoop(event_loop::Loop& loop,
  std::shared_ptr<shell::ManagedShell> shell,shell::RunOptions options,
  bool expectTimeoutCallback){
  if(!shell)throw std::invalid_argument("managed shell required");
  event_loop::Deferred<AsyncShellCommand> ready(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  options.runInBackground=false;options.onProgress={};
  options.managedAssistantBackgroundEnabled=false;
  try{
    ::batbox::runtime::contextThread([shell=std::move(shell),options=std::move(options),
      expectTimeoutCallback,ready,keep,dispatcher,&loop]()mutable{
      std::optional<shell::CommandHandle> command;
      std::exception_ptr error;
      try{command=shell::startManagedCommand(shell,std::move(options),expectTimeoutCallback);}
      catch(...){error=std::current_exception();}
      try{dispatcher.postMission([command=std::move(command),ready,keep,error,&loop]{
        try{if(error)ready.reject(error);
          else ready.resolve(bindAsyncShellCommand(loop,*command));}
        catch(...){ready.reject(std::current_exception());}
        keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();ready.reject(std::current_exception());}
  return ready;
}
} // namespace batbox::bash_gadget
