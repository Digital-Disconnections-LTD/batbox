#include "instructions/hooks/runtime/IndigenousCommand.hpp"
#include "instructions/hooks/runtime/IndigenousCommandPreparation.hpp"
#include "instructions/hooks/runtime/IndigenousCommandProcess.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <memory>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
bool diagnosticEvent(const std::string& event){
  return event=="SessionStart"||event=="Setup"||event=="SessionEnd";
}
bool configuredAsync(const Json& hook){
  return value::truthy(value::property(hook,"async"))||
    value::truthy(value::property(hook,"asyncRewake"));
}
} // namespace
std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)>
indigenousPreparedCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config){
  return [&loop,config=std::move(config)](CommandRequest request){
    if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
    event_loop::Deferred<CommandOutcome> output(loop);
    try{
      if(configuredAsync(request.hook))throw std::invalid_argument(
        "Configured async command hook requires a native registry provider");
      FullCommandRequest prepared;
      prepared.base=request;
      prepared.hookIndex=request.index;
      prepared.skillRoot=request.skillRoot;
      const bool diagnostic=diagnosticEvent(request.hookEvent);
      if(diagnostic&&(!config.nowMillis||!config.logDiagnostics))
        throw std::invalid_argument("Hook lifecycle diagnostics imports required");
      prepareIndigenousHookCommand(loop,config,prepared).then(
        [&loop,output,config,request,diagnostic](
            const IndigenousProcessRequest* process,const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          try{
            const double started=diagnostic?config.nowMillis():0;
            if(diagnostic)config.logDiagnostics("hook_spawn_started",
              Json{{"hook_event_name",request.hookEvent},
                {"index",request.index}});
            spawnHookProcess(loop,*process).then(
              [output,config,request,diagnostic,started](
                  const CommandOutcome* result,const std::exception_ptr& failure){
                try{
                  if(diagnostic)config.logDiagnostics("hook_spawn_completed",
                    Json{{"hook_event_name",request.hookEvent},
                      {"index",request.index},
                      {"duration_ms",config.nowMillis()-started},
                      {"exit_code",failure?1:result->status},
                      {"aborted",failure?false:result->aborted}});
                  if(failure)output.reject(failure);
                  else output.resolve(*result);
                }catch(...){output.reject(std::current_exception());}
              });
          }catch(...){output.reject(std::current_exception());}
        });
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
}
} // namespace batbox::instructions::hooks::runtime
