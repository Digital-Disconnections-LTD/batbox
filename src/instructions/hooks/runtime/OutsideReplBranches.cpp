#include "instructions/hooks/runtime/OutsideReplInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
double timeoutFor(const Json& hook,double fallback){
  const auto value=timeoutSeconds(hook);
  return value&&*value!=0?*value*1000:fallback;
}
HookResult failed(const std::string& command,const std::string& error){
  return {command,error,false,false};
}
}
void OutsideRun::runCallback(const MatchedHook& matched,std::size_t index){
  const std::string command="callback";
  const auto timeout=timeoutFor(matched.hook,dispatch.timeoutMs);
  std::shared_ptr<HookAbort> abort;
  try{
    abort=HookAbort::create(loop,parentSignal,timeout);
    if(!services.randomUUID||!matched.callback)
      throw std::invalid_argument("Hook UUID and callback imports required");
    const auto uuid=services.randomUUID();
    matched.callback(CallbackRequest{dispatch.hookInput,uuid,
      abort->signal(),index}).then(
      [self=shared_from_this(),abort,index](const Json* json,
        const std::exception_ptr& error){
        abort->cleanup();
        if(error){
          const auto message=exceptionMessage(error);
          self->debug(self->hookName+" [callback] failed to run: "+message,"error");
          self->finish(index,failed("callback",message));return;
        }
        try{
          if(isAsyncHookJsonOutput(*json)){
            self->debug(self->hookName+
              " [callback] returned async response, returning empty output");
            self->finish(index,{"callback","",true,false});return;
          }
          std::string output;
          if(self->hookEvent=="WorktreeCreate")
            output=worktreePath(*json).value_or(stringField(*json,"systemMessage"));
          else output=stringField(*json,"systemMessage");
          self->debug(self->hookName+" [callback] completed successfully");
          self->finish(index,{"callback",output,true,syncBlocked(*json)});
        }catch(...){
          const auto message=exceptionMessage(std::current_exception());
          self->debug(self->hookName+" [callback] failed to run: "+message,"error");
          self->finish(index,failed("callback",message));
        }
      });
  }catch(...){
    if(abort)abort->cleanup();
    const auto message=exceptionMessage(std::current_exception());
    debug(hookName+" [callback] failed to run: "+message,"error");
    finish(index,failed(command,message));
  }
}
void OutsideRun::runHttp(const MatchedHook& matched,std::size_t index){
  const auto url=stringField(matched.hook,"url");
  try{
    if(!services.execHttpHook)
      throw std::invalid_argument("HTTP hook executor import required");
    services.execHttpHook(HttpRequest{matched.hook,hookEvent,jsonInput,
      parentSignal}).then(
      [self=shared_from_this(),url,index](const HttpOutcome* result,
        const std::exception_ptr& error){
        try{
          if(error)std::rethrow_exception(error);
          if(result->aborted){
            self->debug(self->hookName+" ["+url+"] cancelled");
            self->finish(index,failed(url,"Hook cancelled"));return;
          }
          if(result->error||!result->ok){
            const auto message=result->error&&!result->error->empty()
              ?*result->error:"HTTP "+
                (result->statusCode?std::to_string(*result->statusCode):"undefined")+
                " from "+url;
            self->debug(self->hookName+" ["+url+"] failed: "+message,"error");
            self->finish(index,failed(url,message));return;
          }
          if(!self->services.parseHttpHookOutput)
            throw std::invalid_argument("HTTP hook JSON parser import required");
          const auto parsed=self->services.parseHttpHookOutput(result->body);
          if(parsed.validationError)throw std::runtime_error(*parsed.validationError);
          const auto json=parsed.json;
          if(json&&!isAsyncHookJsonOutput(*json))
            self->debug("Parsed JSON output from HTTP hook: "+value::stringify(*json),"verbose");
          const bool blocked=json&&syncBlocked(*json);
          const auto output=self->hookEvent=="WorktreeCreate"
            ?(json?worktreePath(*json).value_or(""):""):result->body;
          self->finish(index,{url,output,true,blocked});
        }catch(...){
          const auto message=exceptionMessage(std::current_exception());
          self->debug(self->hookName+" ["+url+"] failed to run: "+message,"error");
          self->finish(index,failed(url,message));
        }
      });
  }catch(...){
    const auto message=exceptionMessage(std::current_exception());
    debug(hookName+" ["+url+"] failed to run: "+message,"error");
    finish(index,failed(url,message));
  }
}
void OutsideRun::runCommand(const MatchedHook& matched,std::size_t index){
  const auto command=stringField(matched.hook,"command");
  const auto timeout=timeoutFor(matched.hook,dispatch.timeoutMs);
  std::shared_ptr<HookAbort> abort;
  try{
    abort=HookAbort::create(loop,parentSignal,timeout);
    if(!services.execCommandHook||!services.randomUUID)
      throw std::invalid_argument("Command hook executor and UUID imports required");
    const auto uuid=services.randomUUID();
    services.execCommandHook(CommandRequest{matched.hook,hookEvent,
      hookName,jsonInput,uuid,index,matched.pluginRoot,matched.pluginId,
      abort->signal(),matched.skillRoot}).then(
      [self=shared_from_this(),abort,command,index](const CommandOutcome* result,
        const std::exception_ptr& error){
        abort->cleanup();
        try{
          if(error)std::rethrow_exception(error);
          if(result->aborted){
            self->debug(self->hookName+" ["+command+"] cancelled");
            self->finish(index,failed(command,"Hook cancelled"));return;
          }
          self->debug(self->hookName+" ["+command+"] completed with status "+
            std::to_string(result->status));
          if(!self->services.parseHookOutput)
            throw std::invalid_argument("Command hook JSON parser import required");
          const auto parsed=self->services.parseHookOutput(result->stdoutText);
          if(parsed.validationError)throw std::runtime_error(*parsed.validationError);
          const auto json=parsed.json;
          if(json&&!isAsyncHookJsonOutput(*json))
            self->debug("Parsed JSON output from hook: "+value::stringify(*json),"verbose");
          HookResult row;
          row.command=command;row.succeeded=result->status==0;
          row.blocked=result->status==2||(json&&syncBlocked(*json));
          row.output=result->status==0?result->stdoutText:result->stderrText;
          if(json&&isSyncHookJsonOutput(*json)){
            row.systemMessage=optionalString(*json,"systemMessage");
            if(json->is_object()&&json->contains("hookSpecificOutput")){
              const auto& specific=json->at("hookSpecificOutput");
              if(specific.is_object()&&specific.contains("watchPaths"))
                row.watchPaths=specific.at("watchPaths").get<std::vector<std::string>>();
            }
          }
          self->finish(index,std::move(row));
        }catch(...){
          const auto message=exceptionMessage(std::current_exception());
          self->debug(self->hookName+" ["+command+"] failed to run: "+message,"error");
          self->finish(index,failed(command,message));
        }
      });
  }catch(...){
    if(abort)abort->cleanup();
    const auto message=exceptionMessage(std::current_exception());
    debug(hookName+" ["+command+"] failed to run: "+message,"error");
    finish(index,failed(command,message));
  }
}
} // namespace batbox::instructions::hooks::runtime
