#include "filesystem/RipgrepProcess.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include "runtime/JsonValue.hpp"
#include <uv.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <memory>
#include <mutex>
#include <system_error>

namespace batbox::filesystem {
namespace {
struct RawResult {
  std::string out,err;
  int exitCode=0,execErrno=0;
  bool timedOut=false,aborted=false,overflow=false;
};
RawResult runRaw(const std::vector<std::string>& args,const std::string& target,
    std::stop_token stopToken,const RipgrepConfig& config,bool singleThread){
  std::vector<std::string> argv{config.command};
  argv.insert(argv.end(),config.prefixArgs.begin(),config.prefixArgs.end());
  if(singleThread){argv.push_back("-j");argv.push_back("1");}
  argv.insert(argv.end(),args.begin(),args.end());argv.push_back(target);
  struct Output {std::mutex mutex;RawResult raw;};
  auto output=std::make_shared<Output>();
  process::IndigenousChildOptions options;
  options.lineMode=false;
  options.onData=[output,limit=config.maxBufferBytes](const std::string& bytes,bool stdoutPipe){
    std::lock_guard lock(output->mutex);
    auto& raw=output->raw;
    auto& into=stdoutPipe?raw.out:raw.err;
    if(into.size()+bytes.size()>limit){
      if(into.size()<limit)into.append(bytes,0,limit-into.size());
      raw.overflow=true;
    }else into+=bytes;
  };
  std::unique_ptr<process::IndigenousJsonlProcess> child;
  try{child=std::make_unique<process::IndigenousJsonlProcess>(argv,
    std::map<std::string,std::string>{},std::move(options));}
  catch(const process::IndigenousChildSpawnError& error){
    RawResult raw;raw.exitCode=-1;
    raw.execErrno=error.uvCode==UV_ENOENT?ENOENT:
      error.uvCode==UV_EACCES?EACCES:
      error.uvCode==UV_EPERM?EPERM:
      error.uvCode==UV_EAGAIN?EAGAIN:0;
    raw.err=error.what();return raw;
  }
  child->closeStdin();
  const auto deadline=std::chrono::steady_clock::now()+
    std::chrono::milliseconds(config.timeoutMs);
  bool killed=false;int exitCode=-1;
  for(;;){
    bool overflow=false;
    {std::lock_guard lock(output->mutex);overflow=output->raw.overflow;}
    if(!killed&&(stopToken.stop_requested()||
       std::chrono::steady_clock::now()>=deadline||overflow)){
      std::lock_guard lock(output->mutex);
      output->raw.aborted=stopToken.stop_requested();
      output->raw.timedOut=!output->raw.aborted&&std::chrono::steady_clock::now()>=deadline;
      child->kill();killed=true;
    }
    try{
      exitCode=child->waitForExit(std::chrono::steady_clock::now()+
        std::chrono::milliseconds(50));break;
    }catch(const std::exception& error){
      if(std::string(error.what())!="Child process wait timed out")throw;
    }
  }
  child->close();
  std::lock_guard lock(output->mutex);
  output->raw.exitCode=exitCode;
  return output->raw;
}
bool eagain(const std::string& error){
  return error.find("os error 11")!=std::string::npos||
    error.find("Resource temporarily unavailable")!=std::string::npos;
}
}
RipgrepLines ripGrep(const std::vector<std::string>& args,
    const std::string& target,std::stop_token stopToken,const RipgrepConfig& config){
  auto raw=runRaw(args,target,stopToken,config,false);
  if(raw.exitCode==0&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return parseRipgrepLines(raw.out,false);
  if(raw.exitCode==1&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return {};
  if(raw.execErrno==EAGAIN||eagain(raw.err))raw=runRaw(args,target,stopToken,config,true);
  if(raw.execErrno){
    if(raw.execErrno==ENOENT||raw.execErrno==EACCES||raw.execErrno==EPERM)
      throw std::system_error(raw.execErrno,std::generic_category(),"ripgrep");
  }
  if(raw.exitCode==0&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return parseRipgrepLines(raw.out,false);
  if(raw.exitCode==1&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return {};
  return finishRipgrep({std::move(raw.out),std::move(raw.err),raw.exitCode,
    raw.timedOut,raw.aborted,raw.overflow},config);
}
} // namespace batbox::filesystem
