#include <filesystem>
#include <cstdlib>
#include "process/ExecFileNoThrow.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>
#include <unistd.h>

using namespace batbox::process;
namespace {
ExecResult invoke(const std::string& file,const std::vector<std::string>& args,
  std::optional<ExecOptions> options={}){
  batbox::event_loop::Loop loop;
  ExecImports imports;imports.execa=indigenousExeca(loop);
  imports.getCwd=[] {return std::string("/tmp");};
  imports.logError=[](std::exception_ptr error){std::rethrow_exception(error);};
  auto pending=execFileNoThrowWithCwd(loop,file,args,options,imports);
  std::optional<ExecResult> result;
  pending.then([&](const ExecResult* value,const std::exception_ptr& error){
    assert(loop.isOwnerThread());if(error)std::rethrow_exception(error);result=*value;});
  loop.run();assert(result);return *result;
}
}
int main(){
  const auto plain=invoke("/usr/bin/printf",{"%s", "literal $HOME ; *"});
  assert(plain.code==0&&plain.stdoutText=="literal $HOME ; *"&&plain.stderrText.empty());
  ExecOptions input;input.stdin="pipe";input.input="abc\n";
  const auto piped=invoke("/bin/cat",{},input);
  assert(piped.code==0&&piped.stdoutText=="abc");
  ExecOptions closedInput;closedInput.stdin="pipe";
  closedInput.input=std::string(1024*1024,'x');
  const auto closed=invoke("/bin/false",{},closedInput);
  assert(closed.code==1);
  ExecOptions cwd;cwd.cwd="/tmp";
  const auto path=invoke("/bin/pwd",{},cwd);
  assert(path.code==0&&std::filesystem::equivalent(path.stdoutText,"/tmp"));
  ExecOptions env;env.env=std::map<std::string,std::string>{{"BATBOX_EXEC_TEST","present"}};
  const auto variable=invoke("/usr/bin/env",{},env);
  assert(variable.code==0&&variable.stdoutText.find("BATBOX_EXEC_TEST=present")!=std::string::npos);
  char directory[]="/tmp/batbox-exec-path-XXXXXX";assert(::mkdtemp(directory));
  const auto command=std::filesystem::path(directory)/"batbox-path-test";
  {std::ofstream file(command);file<<"#!/bin/sh\nprintf path-override\n";}
  std::filesystem::permissions(command,std::filesystem::perms::owner_all);
  ExecOptions pathEnv;pathEnv.env=std::map<std::string,std::string>{{"PATH",directory}};
  const auto viaPath=invoke("batbox-path-test",{},pathEnv);
  assert(viaPath.code==0&&viaPath.stdoutText=="path-override");
  std::filesystem::remove_all(directory);
  const auto failure=invoke("/bin/sh",{"-c","printf partial; exit 7"});
  assert(failure.code==7&&failure.stdoutText=="partial"&&failure.error);
  ExecOptions discard;discard.preserveOutputOnError=false;
  const auto hidden=invoke("/bin/sh",{"-c","printf partial; exit 7"},discard);
  assert(hidden.code==7&&hidden.stdoutText.empty()&&!hidden.error);
  const auto missing=invoke("/tmp/batbox-executable-does-not-exist",{});
  assert(missing.code==1&&missing.error);
  ExecOptions timeout;timeout.timeout=50;
  const auto started=std::chrono::steady_clock::now();
  const auto slow=invoke("/bin/sh",{"-c","sleep 10"},timeout);
  assert(slow.code==1&&slow.error);
  assert(std::chrono::steady_clock::now()-started<std::chrono::seconds(3));
  {
    batbox::event_loop::Loop loop;std::stop_source stop;
    ExecOptions abort;abort.abortSignal=stop.get_token();abort.timeout=30000;
    ExecImports imports;imports.execa=indigenousExeca(loop);
    auto pending=execFileNoThrowWithCwd(loop,"/bin/sh",{"-c","sleep 10"},abort,imports);
    bool finished=false;
    pending.then([&](const ExecResult* value,const std::exception_ptr& error){
      assert(!error&&value->code==1&&value->error);finished=true;});
    loop.setTimeout(50,[&]{stop.request_stop();});
    loop.run();assert(finished);
  }
  std::cout<<"native argv process, stdin, cwd, env, failures, timeout and abort passed\n";
}
