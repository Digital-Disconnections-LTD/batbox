#include "instructions/hooks/runtime/IndigenousManagedHookProcess.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
hook::IndigenousProcessRequest request(std::string command,std::string input="{}"){
  hook::IndigenousProcessRequest value;
  value.shellPath="/bin/sh";value.command=std::move(command);
  value.cwd="/tmp";value.input=std::move(input);
  value.environment={{"PATH","/usr/bin:/bin"}};
  value.timeoutMs=10000;
  return value;
}
}
int main(){
  {
    event_loop::Loop loop;
    std::vector<std::string> stream;
    auto process=hook::startIndigenousManagedHookProcess(loop,request(
      "read line; printf '%s\\n' \"$line\"; printf 'warn\\n' >&2; "
      "sleep 0.03; printf 'tail\\n'","{\"text\":\"é\"}"));
    auto lifetime=process.lifetimeToken();
    assert(process.pid()>0&&process.status()=="running");
    process.onStdout([&](const std::string& text){stream.push_back(text);});
    assert(process.background("async_hook_1"));
    assert(process.status()=="backgrounded");
    int exitCode=-1;
    process.resultCode().then([&](const int* value,const std::exception_ptr& error){
      assert(!error);exitCode=*value;});
    loop.run();
    assert(exitCode==0&&process.status()=="completed");
    std::string stdoutText;
    process.getStdout().then([&](const std::string* value,const std::exception_ptr& error){
      assert(!error);stdoutText=*value;});
    loop.run();
    assert(stdoutText=="{\"text\":\"é\"}\ntail\n");
    assert(process.getStderr()=="warn\n");
    std::string collected;for(const auto& text:stream)collected+=text;
    assert(collected==stdoutText);
    process.cleanup();
    std::string cleared="not-cleared";
    process.getStdout().then([&](const std::string* value,const std::exception_ptr& error){
      assert(!error);cleared=*value;});
    loop.run();
    assert(cleared.empty()&&process.getStderr().empty());
    assert(!process.background("too-late"));
    process=hook::IndigenousManagedHookProcess(nullptr);
    assert(lifetime.expired());
  }
  {
    event_loop::Loop loop;
    auto process=hook::startIndigenousManagedHookProcess(loop,request(
      "printf '\\303'; sleep 0.03; printf '\\251\\n'"));
    std::string streamed;
    process.onStdout([&](const std::string& text){streamed+=text;});
    loop.run();
    assert(streamed=="é\n");
    assert(process.getStderr().empty());
  }
  {
    event_loop::Loop loop;
    auto process=hook::startIndigenousManagedHookProcess(loop,request(
      "read input; printf '{\"prompt\":\"id\",\"message\":\"Choose\",\"options\":[]}\\n'; "
      "read reply; printf '%s\\n' \"$reply\""),true);
    bool wrote=false;std::string stdoutText;
    process.onStdout([&](const std::string& text){
      stdoutText+=text;
      if(!wrote&&stdoutText.find('\n')!=std::string::npos){
        wrote=true;
        process.writeStdin("{\"prompt_response\":\"id\",\"selected\":\"yes\"}\n");
      }
    });
    loop.run();
    assert(wrote);
    assert(stdoutText.find("{\"prompt_response\":\"id\",\"selected\":\"yes\"}")!=std::string::npos);
  }
  {
    event_loop::Loop loop;
    auto process=hook::startIndigenousManagedHookProcess(loop,request("sleep 10"));
    loop.setTimeout(35,[process]{process.kill();});
    loop.run();
    assert(process.status()=="killed");
    assert(process.resultCode().settled());
    int resultCode=-1,closeCode=-1;
    process.resultCode().then([&](const int* value,const std::exception_ptr& error){
      assert(!error);resultCode=*value;});
    process.closeCode().then([&](const int* value,const std::exception_ptr& error){
      assert(!error);closeCode=*value;});
    loop.run();
    assert(resultCode==137&&closeCode==1);
  }
  {
    event_loop::Loop loop;
    auto invocation=request("sleep 10");invocation.timeoutMs=35;
    auto process=hook::startIndigenousManagedHookProcess(loop,std::move(invocation));
    loop.run();
    int resultCode=-1,closeCode=-1;
    process.resultCode().then([&](const int* value,const std::exception_ptr& error){
      assert(!error);resultCode=*value;});
    process.closeCode().then([&](const int* value,const std::exception_ptr& error){
      assert(!error);closeCode=*value;});
    loop.run();
    assert(process.status()=="killed"&&resultCode==143&&closeCode==1);
  }
  {
    event_loop::Loop loop;
    std::stop_source stopped;stopped.request_stop();
    auto invocation=request("sleep 0.03; printf 'survived\\n'");
    invocation.signal=stopped.get_token();
    auto process=hook::startIndigenousManagedHookProcess(loop,std::move(invocation),
      false,[]{return std::string("interrupt");});
    loop.run();
    assert(process.status()=="completed");
    std::string stdoutText;
    process.getStdout().then([&](const std::string* text,const std::exception_ptr& error){
      assert(!error);stdoutText=*text;});
    loop.run();
    assert(stdoutText=="survived\n");
  }
  std::cout<<"native async hook process streams, stdin, UTF-8, kill and lifetime pass\n";
}
