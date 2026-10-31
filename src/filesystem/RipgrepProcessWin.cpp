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
std::vector<std::string> parseLines(const std::string& output,bool dropLast){
  const auto trimmed=value::trim(output);
  std::vector<std::string> lines;
  std::size_t start=0;
  while(start<=trimmed.size()){
    const auto end=trimmed.find('\n',start);
    std::string line=trimmed.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(!line.empty())lines.push_back(std::move(line));
    if(end==std::string::npos)break;
    start=end+1;
  }
  if(dropLast&&!lines.empty())lines.pop_back();
  return lines;
}
bool eagain(const std::string& error){
  return error.find("os error 11")!=std::string::npos||
    error.find("Resource temporarily unavailable")!=std::string::npos;
}
}
std::vector<std::string> ripGrep(const std::vector<std::string>& args,
    const std::string& target,std::stop_token stopToken,const RipgrepConfig& config){
  auto raw=runRaw(args,target,stopToken,config,false);
  if(raw.exitCode==0&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return parseLines(raw.out,false);
  if(raw.exitCode==1&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return {};
  if(raw.execErrno==EAGAIN||eagain(raw.err))raw=runRaw(args,target,stopToken,config,true);
  if(raw.execErrno){
    if(raw.execErrno==ENOENT||raw.execErrno==EACCES||raw.execErrno==EPERM)
      throw std::system_error(raw.execErrno,std::generic_category(),"ripgrep");
  }
  if(raw.exitCode==0&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return parseLines(raw.out,false);
  if(raw.exitCode==1&&!raw.timedOut&&!raw.aborted&&!raw.overflow)return {};
  const bool timeout=raw.timedOut||raw.aborted;
  auto lines=parseLines(raw.out,timeout||raw.overflow);
  if(timeout&&lines.empty())
    throw RipgrepTimeoutError("Ripgrep search timed out after "+
      std::to_string(config.timeoutDisplaySeconds)+
      " seconds. The search may have matched files but did not complete in time. Try searching a more specific path or pattern.",
      lines);
  return lines;
}
} // namespace batbox::filesystem
