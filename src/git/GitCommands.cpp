#include "git/GitCommands.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::git {
namespace {
std::vector<std::string> splitLines(const std::string& text){
  std::vector<std::string> lines;std::size_t start=0;
  for(;;){const auto end=text.find('\n',start);
    if(end==std::string::npos){lines.push_back(text.substr(start));break;}
    lines.push_back(text.substr(start,end-start));start=end+1;
  }
  return lines;
}
template<class T,class F>event_loop::Deferred<T> transform(
  event_loop::Loop& loop,event_loop::Deferred<process::ExecResult> input,F&& fn){
  event_loop::Deferred<T> output(loop);
  input.then([output,fn=std::forward<F>(fn)](const process::ExecResult* value,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    try{output.resolve(fn(*value));}catch(...){output.reject(std::current_exception());}
  });
  return output;
}
}
GitCommands::GitCommands(event_loop::Loop& loop,CommandImports imports):
  loop_(loop),imports_(std::move(imports)){
  if(!imports_.gitExe||!imports_.execFileNoThrow)
    throw std::invalid_argument("Git command imports required");
}
event_loop::Deferred<process::ExecResult> GitCommands::run(
  const std::vector<std::string>& args){
  if(!loop_.isOwnerThread())throw std::logic_error("Git command owner required");
  process::ExecOptions options;options.preserveOutputOnError=false;
  return imports_.execFileNoThrow(imports_.gitExe(),args,options);
}
event_loop::Deferred<bool> GitCommands::getIsHeadOnRemote(){
  try{return transform<bool>(loop_,run({"rev-parse","@{u}"}),
    [](const process::ExecResult& result){return result.code==0;});}
  catch(...){event_loop::Deferred<bool> result(loop_);result.reject(std::current_exception());return result;}
}
event_loop::Deferred<bool> GitCommands::hasUnpushedCommits(){
  try{return transform<bool>(loop_,run({"rev-list","--count","@{u}..HEAD"}),
    [](const process::ExecResult& result){
      return result.code==0&&value::parseInt10(value::trim(result.stdoutText))>0;});}
  catch(...){event_loop::Deferred<bool> result(loop_);result.reject(std::current_exception());return result;}
}
event_loop::Deferred<bool> GitCommands::getIsClean(bool ignoreUntracked){
  try{
    std::vector<std::string> args{"--no-optional-locks","status","--porcelain"};
    if(ignoreUntracked)args.push_back("-uno");
    return transform<bool>(loop_,run(args),[](const process::ExecResult& result){
      return value::trim(result.stdoutText).empty();});
  }catch(...){event_loop::Deferred<bool> result(loop_);result.reject(std::current_exception());return result;}
}
event_loop::Deferred<std::vector<std::string>> GitCommands::getChangedFiles(){
  try{return transform<std::vector<std::string>>(loop_,
    run({"--no-optional-locks","status","--porcelain"}),
    [](const process::ExecResult& result){
      std::vector<std::string> files;
      for(const auto& line:splitLines(value::trim(result.stdoutText))){
        const auto trimmed=value::trim(line);
        const auto firstSpace=trimmed.find(' ');
        if(firstSpace!=std::string::npos){
          const auto secondSpace=trimmed.find(' ',firstSpace+1);
          files.push_back(value::trim(trimmed.substr(firstSpace+1,
            secondSpace==std::string::npos?std::string::npos:secondSpace-firstSpace-1)));
        }
      }
      return files;
    });}
  catch(...){event_loop::Deferred<std::vector<std::string>> result(loop_);
    result.reject(std::current_exception());return result;}
}
event_loop::Deferred<GitFileStatus> GitCommands::getFileStatus(){
  try{return transform<GitFileStatus>(loop_,
    run({"--no-optional-locks","status","--porcelain"}),
    [](const process::ExecResult& result){
      GitFileStatus status;
      for(const auto& line:splitLines(value::trim(result.stdoutText))){
        if(line.empty())continue;
        const auto marker=value::substring(line,0,2);
        const auto filename=value::trim(value::substring(line,2));
        if(marker=="??")status.untracked.push_back(filename);
        else if(!filename.empty())status.tracked.push_back(filename);
      }
      return status;
    });}
  catch(...){event_loop::Deferred<GitFileStatus> result(loop_);
    result.reject(std::current_exception());return result;}
}
} // namespace batbox::git
