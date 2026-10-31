#include "runtime/AsyncLaunch.hpp"
#include "shell/AsyncCommandResult.hpp"
#include <memory>
#include <optional>
#include <thread>

namespace batbox::shell {
event_loop::Deferred<ExecResult> resultOnLoop(event_loop::Loop& loop,
  std::shared_future<ExecResult> future){
  if(!future.valid())throw std::invalid_argument("valid command result required");
  event_loop::Deferred<ExecResult> completed(loop);
  const auto dispatcher=loop.dispatcher();
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  try{
    ::batbox::runtime::contextThread([future=std::move(future),completed,dispatcher,keep]{
      std::optional<ExecResult> value;
      std::exception_ptr error;
      try{value=future.get();}catch(...){error=std::current_exception();}
      try{dispatcher.postMission([completed,keep,value=std::move(value),error]{
        if(error)completed.reject(error);else completed.resolve(*value);
        keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();completed.reject(std::current_exception());}
  return completed;
}
} // namespace batbox::shell
