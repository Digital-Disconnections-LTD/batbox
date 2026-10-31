#include "instructions/hooks/runtime/IndigenousCommand.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <map>
#include <stop_token>
#include <string>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
int main(){
  event_loop::Loop loop;
  hook::IndigenousCommandConfig config;
  config.getProjectRoot=[](){return std::string("/tmp");};
  config.getCwd=[](){return std::string("/tmp");};
  config.getOriginalCwd=config.getCwd;
  config.subprocessEnv=[](){return std::map<std::string,std::string>{{"PATH","/usr/bin:/bin"}};};
  config.pathExists=[&loop](const std::string& path){
    event_loop::Deferred<bool> result(loop);
    result.resolve(std::filesystem::exists(path));return result;
  };
  config.shellPrefix=[](){return std::optional<std::string>{};};
  auto execute=hook::indigenousCommandHook(loop,config);
  std::stop_source cancel;
  hook::CommandRequest request;
  request.hook={{"type","command"},{"command","sleep 5"}};
  request.hookEvent="InstructionsLoaded";
  request.jsonInput="{}";
  request.signal=cancel.get_token();
  bool settled=false,aborted=false;
  execute(request).then([&](const hook::CommandOutcome* result,
    const std::exception_ptr& error){
    assert(!error);assert(result);settled=true;aborted=result->aborted;
  });
  loop.setTimeout(40,[&]{cancel.request_stop();});
  const auto started=std::chrono::steady_clock::now();
  loop.run();
  assert(settled&&aborted);
  assert(std::chrono::steady_clock::now()-started<std::chrono::seconds(2));
  std::stop_source fresh;
  request.signal=fresh.get_token();
  request.hook["timeout"]=0.05;
  settled=false;aborted=false;
  execute(request).then([&](const hook::CommandOutcome* result,
    const std::exception_ptr& error){
    assert(!error);assert(result);settled=true;aborted=result->aborted;
  });
  const auto next=std::chrono::steady_clock::now();
  loop.run();
  assert(settled&&!aborted);
  assert(std::chrono::steady_clock::now()-next<std::chrono::seconds(2));
}
