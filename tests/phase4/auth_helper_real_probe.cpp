#include "auth/helper/IndigenousCommand.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  event_loop::Loop loop;
  const auto execute=[&](const std::string& command,double timeout){
    std::optional<auth::helper::CommandResult> result;
    auto promise=auth::helper::indigenousExecuteShell(loop,command,timeout);
    promise.then([&](const auth::helper::CommandResult* value,
        const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      result=*value;
    });
    loop.run();
    if(!result)throw std::runtime_error("Native helper command did not settle");
    return *result;
  };
  const auto success=execute("printf 'token'; printf 'notice' >&2",1000);
  assert(!success.failed&&success.stdoutText=="token"&&
    success.stderrText=="notice"&&success.exitCode==0);
  const auto failure=execute("printf 'bad' >&2; exit 7",1000);
  assert(failure.failed&&failure.stderrText=="bad"&&failure.exitCode==7);
  const auto before=std::chrono::steady_clock::now();
  const auto timeout=execute("sleep 5",40);
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now()-before).count();
  assert(timeout.failed&&timeout.timedOut&&elapsed<2000);
  std::cout<<"Native helper command fixture passed\n";
}
