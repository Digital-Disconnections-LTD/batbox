#include "instructions/hooks/runtime/AsyncHookDetection.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
AsyncHookDetection::AsyncHookDetection(event_loop::Loop& loop,
  BackgroundHookRequest request,AsyncDetectionServices services,bool forceSyncExecution):
  loop_(loop),request_(std::move(request)),services_(std::move(services)),
  forceSyncExecution_(forceSyncExecution),asyncResult_(loop){
  if(!services_.logDebug||!services_.parseJson||!services_.stringifyJson||
    !services_.stringifyThrown||!services_.executeBackground)
    throw std::invalid_argument("Async hook detection imports required");
}
void AsyncHookDetection::onStderr(const std::string& data){
  if(!loop_.isOwnerThread())throw std::logic_error("Hook stream owner required");
  stderr_+=data;output_+=data;
}
void AsyncHookDetection::onStdout(const std::string& data){
  if(!loop_.isOwnerThread())throw std::logic_error("Hook stream owner required");
  stdout_+=data;output_+=data;
  if(initialResponseChecked_)return;
  const auto newline=stdout_.find('\n');
  const auto firstLine=value::trim(stdout_.substr(0,newline));
  if(firstLine.find('}')==std::string::npos)return;
  initialResponseChecked_=true;
  services_.logDebug("Hooks: Checking first line for async: "+firstLine);
  try{
    const auto parsed=services_.parseJson(firstLine);
    services_.logDebug("Hooks: Parsed initial response: "+
      services_.stringifyJson(parsed));
    if(isAsyncHookJsonOutput(parsed)&&!forceSyncExecution_){
      services_.logDebug("Hooks: Detected async hook, backgrounding process "+
        request_.processId);
      auto request=request_;request.asyncResponse=parsed;
      if(services_.executeBackground(std::move(request))){
        transferred_=true;
        asyncResult_.resolve({stdout_,stderr_,output_,0,false,false});
      }
    }else if(isAsyncHookJsonOutput(parsed)&&forceSyncExecution_){
      services_.logDebug("Hooks: Detected async hook but forceSyncExecution is true, waiting for completion");
    }else{
      services_.logDebug("Hooks: Initial response is not async, continuing normal processing");
    }
  }catch(...){
    services_.logDebug("Hooks: Failed to parse initial response as JSON: "+
      services_.stringifyThrown(std::current_exception()));
  }
}
} // namespace batbox::instructions::hooks::runtime
