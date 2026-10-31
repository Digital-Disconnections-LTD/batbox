#include "instructions/hooks/runtime/FlightEndHooks.hpp"
#include "runtime/Encoding.hpp"
#include <algorithm>
#include <cerrno>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace batbox::instructions::hooks::runtime {
event_loop::Deferred<event_loop::Unit> executeFlightEndHooks(
  event_loop::Loop& loop,const FlightEndHookServices& services,
  const std::string& reason,FlightEndOptions options){
  if(!loop.isOwnerThread())throw std::logic_error("Session end hook owner required");
  if(!services.createBaseHookInput||!services.executeHooksOutsideRepl||
    !services.writeStderr)
    throw std::invalid_argument("Session end hook imports required");
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    const auto base=services.createBaseHookInput();
    HookDispatch dispatch;
    dispatch.hookInput=base.toJson();
    if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
    if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
    if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
    dispatch.hookInput["hook_event_name"]="SessionEnd";
    dispatch.hookInput["reason"]=reason;
    dispatch.matchQuery=reason;
    dispatch.timeoutMs=options.timeoutMs.value_or(services.defaultTimeoutMs);
    services.executeHooksOutsideRepl(std::move(dispatch),options.getAppState,
      options.signal).then([output,services,options](
      const std::vector<HookResult>* results,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        for(const auto& result:*results)
          if(!result.succeeded&&!result.output.empty())
            services.writeStderr("SessionEnd hook ["+result.command+"] failed: "+
              result.output+"\n");
        if(options.setAppState){
          if(!services.getFlightId||!services.clearFlightHooks)
            throw std::invalid_argument("Session hook clearing imports required");
          const auto flightId=services.getFlightId();
          services.clearFlightHooks(options.setAppState,flightId);
        }
        output.resolve({});
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void indigenousFlightEndStderrWrite(const std::string& text){
  const auto bytes=value::encodeUtf8(text);
  std::size_t offset=0;
  while(offset<bytes.size()){
#ifdef _WIN32
    const HANDLE output=::GetStdHandle(STD_ERROR_HANDLE);
    const auto wanted=static_cast<DWORD>(std::min<std::size_t>(
      bytes.size()-offset,0xffffffffu));
    DWORD count=0;
    if(output==INVALID_HANDLE_VALUE||output==nullptr||
      !::WriteFile(output,bytes.data()+offset,wanted,&count,nullptr))
      throw std::system_error(static_cast<int>(::GetLastError()),
        std::system_category(),"stderr write");
#else
    const auto count=::write(STDERR_FILENO,bytes.data()+offset,bytes.size()-offset);
    if(count<0){
      if(errno==EINTR)continue;
      throw std::system_error(errno,std::generic_category(),"stderr write");
    }
#endif
    if(count==0)throw std::runtime_error("stderr write returned zero bytes");
    offset+=static_cast<std::size_t>(count);
  }
}
} // namespace batbox::instructions::hooks::runtime
