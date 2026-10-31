#include "context/AsyncContext.hpp"
#include "process/ExecFileNoThrow.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,const event_loop::Deferred<T>& promise){
  std::optional<T> value;std::exception_ptr error;
  promise.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value.emplace(*result);loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);assert(value);return *value;
}
}
int main(){
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("batbox-context-"+std::to_string(::getpid()));
  fs::create_directories(root);
  event_loop::Loop loop;
  process::ExecImports imports;
  imports.getCwd=[&]{assert(loop.isOwnerThread());return root.string();};
  imports.logError=[](std::exception_ptr){assert(false);};
  imports.execa=process::indigenousExeca(loop);
  auto execute=[&](const std::vector<std::string>& args){
    process::ExecOptions options;options.useCwd=true;options.preserveOutputOnError=false;
    return process::execFileNoThrow(loop,"git",args,options,imports);
  };
  for(const auto& args:std::vector<std::vector<std::string>>{
      {"init","-b","main"},{"config","user.name","Batbox Fixture"},
      {"config","user.email","fixture@example.test"},{"commit","--allow-empty","-m","seed commit"}})
    assert(await(loop,execute(args)).code==0);
  std::ofstream(root/"new-file.txt")<<"untracked\n";
  auto stdoutFor=[&](const std::vector<std::string>& args){
    event_loop::Deferred<std::string> result(loop);
    execute(args).then([result,&loop](const process::ExecResult* value,const std::exception_ptr& error){
      assert(loop.isOwnerThread());if(error)result.reject(error);else result.resolve(value->stdoutText);
    });return result;
  };
  context::AsyncServices services;
  int gitChecks=0,gitCommands=0;
  services.getIsGit=[&]{
    ++gitChecks;event_loop::Deferred<bool> result(loop);
    execute({"rev-parse","--is-inside-work-tree"}).then(
      [result](const process::ExecResult* value,const std::exception_ptr& error){
        if(error)result.reject(error);else result.resolve(value->code==0);
      });return result;
  };
  services.getBranch=[&]{return stdoutFor({"branch","--show-current"});};
  services.getDefaultBranch=[&]{event_loop::Deferred<std::string> value(loop);value.resolve("main");return value;};
  services.gitStdoutNoThrow=[&](const std::vector<std::string>& args){++gitCommands;return stdoutFor(args);};
  services.includeGitInstructions=[]{return true;};services.stashBreakerFeature=[]{return true;};
  services.logError=[](std::exception_ptr){assert(false);};
  context::AsyncContextStash stash(loop);
  auto first=stash.getSystemContext(services),second=stash.getSystemContext(services);
  assert(first.samePromise(second));assert(!first.settled());
  bool progressed=false;loop.queueMicrotask([&]{progressed=true;});
  const auto context=await(loop,first);assert(progressed);
  const auto status=context.at("gitStatus").get<std::string>();
  assert(status.find("Current branch: main")!=std::string::npos);
  assert(status.find("Git user: Batbox Fixture")!=std::string::npos);
  assert(status.find("?? new-file.txt")!=std::string::npos);
  assert(status.find("seed commit")!=std::string::npos);
  assert(gitChecks==1&&gitCommands==3);
  stash.setSystemPoetryInjection("new prefix");
  const auto updated=await(loop,stash.getSystemContext(services));
  assert(updated.at("gitStatus")==context.at("gitStatus"));
  assert(updated.at("cacheBreaker")=="[CACHE_BREAKER: new prefix]");
  assert(gitChecks==1&&gitCommands==3);
  services.environment["BATBOX_DISABLE_BATBOX_MDS"]="true";
  services.setStashedBatboxMdContent=[&](const std::optional<std::string>& value){assert(loop.isOwnerThread()&&!value);};
  services.localISODate=[]{return "2026-10-28";};
  const auto user=await(loop,stash.getUserContext(services));
  assert(user.at("currentDate")=="Today's date is 2026-10-28.");
  fs::remove_all(root);
}
