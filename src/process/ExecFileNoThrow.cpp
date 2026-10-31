#include "process/ExecFileNoThrow.hpp"
#include <stdexcept>

namespace batbox::process {
Json ExecResult::toJson()const{
  Json value={{"stdout",stdoutText},{"stderr",stderrText},{"code",code}};
  if(error)value["error"]=*error;
  return value;
}
std::string getErrorMessage(const ExecaResult& result,int errorCode){
  if(result.shortMessage&&!result.shortMessage->empty())return *result.shortMessage;
  if(result.signal)return *result.signal;
  return std::to_string(errorCode);
}
event_loop::Deferred<ExecResult> execFileNoThrowWithCwd(event_loop::Loop& loop,
  const std::string& file,const std::vector<std::string>& args,
  std::optional<ExecOptions> options,const ExecImports& imports){
  if(!loop.isOwnerThread())throw std::logic_error("execFileNoThrow owner required");
  event_loop::Deferred<ExecResult> output(loop);
  if(!imports.execa){output.reject(std::make_exception_ptr(
    std::invalid_argument("execa import required")));return output;}
  ExecOptions finalOptions=options.value_or(ExecOptions{});
  if(!options)finalOptions.maxBuffer=1000000;
  if(!finalOptions.timeout)finalOptions.timeout=600000;
  if(!finalOptions.preserveOutputOnError)finalOptions.preserveOutputOnError=true;
  try{
    imports.execa({file,args,finalOptions}).then([output,imports,dispatcher=loop.dispatcher(),
      preserve=*finalOptions.preserveOutputOnError](
      const ExecaResult* result,const std::exception_ptr& error){
      if(error){
        // Propagate rejection through the first Promise before running the catch
        // reaction on the owner loop.
        dispatcher.queueMicrotask([output,imports,error]{
          try{if(imports.logError)imports.logError(error);}
          catch(...){return;}
          output.resolve({"","",1,{}});
        });
        return;
      }
      if(result->failed){
        const int code=result->exitCode.value_or(1);
        if(preserve)output.resolve({result->stdoutText,result->stderrText,code,
          getErrorMessage(*result,code)});
        else output.resolve({"","",code,{}});
      }else output.resolve({result->stdoutText,result->stderrText,0,{}});
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<ExecResult> execFileNoThrow(event_loop::Loop& loop,
  const std::string& file,const std::vector<std::string>& args,
  std::optional<ExecOptions> options,const ExecImports& imports){
  if(!loop.isOwnerThread())throw std::logic_error("execFileNoThrow owner required");
  ExecOptions wrapper=options.value_or(ExecOptions{});
  if(!options){wrapper.timeout=600000;wrapper.preserveOutputOnError=true;wrapper.useCwd=true;}
  ExecOptions forwarded;
  forwarded.abortSignal=wrapper.abortSignal;
  forwarded.timeout=wrapper.timeout;
  forwarded.preserveOutputOnError=wrapper.preserveOutputOnError;
  if(wrapper.useCwd.value_or(false)){
    if(!imports.getCwd)throw std::invalid_argument("getCwd import required");
    forwarded.cwd=imports.getCwd();
  }
  forwarded.env=wrapper.env;forwarded.stdin=wrapper.stdin;forwarded.input=wrapper.input;
  return execFileNoThrowWithCwd(loop,file,args,forwarded,imports);
}
} // namespace batbox::process
