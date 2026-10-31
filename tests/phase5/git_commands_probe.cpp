#include "git/GitCommands.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>

using namespace batbox;
namespace {
template<class T,class Convert>Json observe(event_loop::Loop& loop,event_loop::Deferred<T> future,
  Convert convert){
  std::optional<Json> value;
  future.then([&](const T* result,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);value=convert(*result);});
  loop.runReady();if(!value)throw std::runtime_error("pending Git command");
  return *value;
}
Json run(const Json& item){
  event_loop::Loop loop;Json logs=Json::array(),records=Json::array();
  git::CommandImports imports;
  imports.gitExe=[&]{logs.push_back("gitExe");return "/usr/bin/git";};
  imports.execFileNoThrow=[&](const std::string& exe,
    const std::vector<std::string>& args,const process::ExecOptions& options){
    logs.push_back({{"exe",exe},{"args",args},
      {"options",{{"preserveOutputOnError",options.preserveOutputOnError.value()}}}});
    process::ExecResult result;
    if(args[0]=="rev-parse")result.code=item.at("headCode").get<int>();
    else if(args[0]=="rev-list"){
      result.code=item.at("listCode").get<int>();
      result.stdoutText=item.at("listText").get<std::string>();
    }else{
      result.code=item.at("statusCode").get<int>();
      result.stdoutText=item.at("statusText").get<std::string>();
    }
    event_loop::Deferred<process::ExecResult> pending(loop);pending.resolve(result);return pending;
  };
  git::GitCommands commands(loop,imports);
  Json order=Json::array();auto head=commands.getIsHeadOnRemote();
  std::optional<bool> headValue;
  head.then([&](const bool* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);headValue=*value;order.push_back("head-done");});
  loop.queueMicrotask([&]{order.push_back("sibling");});loop.runReady();
  if(!headValue)throw std::runtime_error("head pending");
  records.push_back({{"op","getIsHeadOnRemote"},{"value",*headValue},{"order",order}});
  records.push_back({{"op","hasUnpushedCommits"},{"value",observe(loop,
    commands.hasUnpushedCommits(),[](bool value)->Json{return value;})}});
  records.push_back({{"op","getIsClean"},{"value",observe(loop,
    commands.getIsClean(),[](bool value)->Json{return value;})}});
  records.push_back({{"op","getIsCleanIgnore"},{"value",observe(loop,
    commands.getIsClean(true),[](bool value)->Json{return value;})}});
  records.push_back({{"op","getChangedFiles"},{"value",observe(loop,
    commands.getChangedFiles(),[](const std::vector<std::string>& value)->Json{return value;})}});
  records.push_back({{"op","getFileStatus"},{"value",observe(loop,
    commands.getFileStatus(),[](const git::GitFileStatus& value)->Json{
      return {{"tracked",value.tracked},{"untracked",value.untracked}};})}});
  return {{"name",item.at("name")},{"records",records},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json out=Json::array();for(const auto& item:value::parse(data))out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
