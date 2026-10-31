#include "instructions/hooks/runtime/IndigenousCommandProcess.hpp"
#include "connectors/Abort.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/Encoding.hpp"
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
CommandOutcome execute(IndigenousProcessRequest request){
  for(const auto* value:{&request.shellPath,&request.command,&request.cwd,&request.input})
    if(value->find('\0')!=std::string::npos)
      throw std::invalid_argument("Hook process values cannot contain NUL");
  for(const auto& [name,value]:request.environment)
    if(name.find('\0')!=std::string::npos||name.find('=')!=std::string::npos||
       value.find('\0')!=std::string::npos)
      throw std::invalid_argument("Invalid hook environment entry");
  struct Output {std::string stdoutRaw,stderrRaw,combined;};
  auto output=std::make_shared<Output>();
  process::IndigenousChildOptions options;
  options.lineMode=false;options.inheritEnvironment=false;
  if(!request.cwd.empty())options.cwd=value::encodeUtf8(request.cwd);
  options.onData=[output](const std::string& bytes,bool stdoutPipe){
    if(stdoutPipe)output->stdoutRaw+=bytes;else output->stderrRaw+=bytes;
    output->combined+=bytes;
  };
  const auto started=std::chrono::steady_clock::now();
  const auto deadline=request.timeoutMs>0?
    started+std::chrono::milliseconds(static_cast<long long>(request.timeoutMs)):
    std::chrono::steady_clock::time_point::max();
  std::vector<std::string> argv{value::encodeUtf8(request.shellPath)};
  if(request.shellType=="powershell"){
    argv.insert(argv.end(),{"-NoProfile","-NonInteractive","-Command"});
  }else argv.push_back("-c");
  argv.push_back(value::encodeUtf8(request.command));
  process::IndigenousJsonlProcess child(argv,request.environment,std::move(options));
  int code=1;bool interrupted=false,inputFailed=false;
  std::exception_ptr failure;
  try{
    child.writeLine(value::encodeUtf8(request.input)+"\n",deadline,request.signal);
    child.closeStdin();
    code=child.waitForExit(deadline,request.signal);
  }catch(const ConnectorAbortError&){
    interrupted=true;child.kill();
    try{(void)child.waitForExit(std::chrono::steady_clock::now()+
      std::chrono::seconds(1));}catch(...){}
  }catch(const std::exception& error){
    const std::string message=error.what();
    inputFailed=message.find("broken pipe")!=std::string::npos||
      message.find("EPIPE")!=std::string::npos;
    const bool timedOut=deadline!=std::chrono::steady_clock::time_point::max()&&
      std::chrono::steady_clock::now()>=deadline;
    if(!inputFailed&&!timedOut)failure=std::current_exception();
    child.kill();
    try{(void)child.waitForExit(std::chrono::steady_clock::now()+
      std::chrono::seconds(1));}catch(...){}
  }
  child.close();
  if(failure)std::rethrow_exception(failure);
  if(inputFailed){
    const std::string message="Hook command closed stdin before hook input was fully written (EPIPE)";
    return {"",message,message,1,false,false};
  }
  return {value::decodeUtf8(output->stdoutRaw),value::decodeUtf8(output->stderrRaw),
    value::decodeUtf8(output->combined),code,interrupted,false};
}
}
event_loop::Deferred<CommandOutcome> spawnHookProcess(
    event_loop::Loop& loop,IndigenousProcessRequest request){
  if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
  event_loop::Deferred<CommandOutcome> result(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([result,request=std::move(request),dispatcher,keep]()mutable{
      std::optional<CommandOutcome> value;std::exception_ptr error;
      try{value=execute(std::move(request));}catch(...){error=std::current_exception();}
      try{dispatcher.postMission([result,value=std::move(value),error,keep]{
        if(error)result.reject(error);else result.resolve(*value);
        keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::instructions::hooks::runtime
