#include "cli/remote/RemoteIO.hpp"
#include <exception>
#include <memory>
#include <utility>

namespace batbox::cli::remote {
Deferred<Unit> RemoteIO::write(const Json& message){
  Deferred<Unit> output(loop_);
  try{
    auto pending=ccr_?ccr_->writeEvent(message):transport_->write(message);
    const bool bridge=bridge_,debug=debug_;
    const auto writeStdout=services_.writeToStdout;
    const auto stringify=services_.ndjsonSafeStringify;
    pending.then([output,message,bridge,debug,writeStdout,stringify](const Unit*,
                                       const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        if(bridge&&(message.value("type",std::string())=="control_request"||
                    debug))
          writeStdout(stringify(message)+"\n");
        output.resolve({});
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
Deferred<Unit> RemoteIO::flushInternalEvents(){
  if(ccr_)return ccr_->flushInternalEvents();
  Deferred<Unit> output(loop_);output.resolve({});return output;
}
std::size_t RemoteIO::internalEventsPending()const{
  return ccr_?ccr_->internalEventsPending():0;
}
void RemoteIO::scheduleKeepAlive(double intervalMs){
  const std::weak_ptr<bool> alive=alive_;
  keepAliveTimer_=loop_.setTimeout(intervalMs,[this,alive,intervalMs]{
    if(alive.expired()||closed_)return;
    services_.logForDebugging("[remote-io] keep_alive sent",nullptr);
    write(Json{{"type","keep_alive"}}).then([this,alive](
        const Unit*,const std::exception_ptr& error){
      if(alive.expired()||!error)return;
      std::string text="unknown";
      try{std::rethrow_exception(error);}
      catch(const std::exception& cause){text=cause.what();}catch(...){}
      services_.logForDebugging(
        "[remote-io] keep_alive write failed: "+text,nullptr);
    });
    scheduleKeepAlive(intervalMs);
  },false);
}
void RemoteIO::close(){
  if(closed_)return;
  closed_=true;
  if(keepAliveTimer_.valid())keepAliveTimer_.cancel();
  if(transport_)transport_->close();
  if(input_)input_->end();
}
void RemoteIO::beginInitialPoetry(){
  const std::weak_ptr<bool> alive=alive_;
  auto next=std::make_shared<std::function<void()>>();
  *next=[this,alive,next]{
    if(alive.expired()||closed_)return;
    services_.nextInitialPoetryChunk().then([this,alive,next](
        const std::optional<std::string>* chunk,
        const std::exception_ptr& error){
      if(alive.expired()||closed_)return;
      if(error)std::rethrow_exception(error);
      if(!chunk||!*chunk){*next={};return;}
      std::string text=**chunk;
      if(!text.empty()&&text.back()=='\n')text.pop_back();
      input_->write(text+"\n");
      (*next)();
    });
  };
  (*next)();
}
}
