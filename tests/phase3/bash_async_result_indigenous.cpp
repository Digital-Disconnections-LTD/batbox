#include <cstdlib>
#include "shell/AsyncCommandResult.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::event_loop;
using namespace batbox::shell;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  {
    Loop loop;std::promise<ExecResult> producer;
    auto result=resultOnLoop(loop,producer.get_future().share());
    bool observed=false;
    result.then([&](const ExecResult* value,const std::exception_ptr& error){
      require(loop.isOwnerThread(),"command observer left owner strand");
      require(!error&&value&&value->stdoutText=="ready"&&value->code==7,
        "command result changed during bridge");
      observed=true;
    });
    std::thread worker([&]{
      std::this_thread::sleep_for(std::chrono::milliseconds(12));
      ExecResult value;value.stdoutText="ready";value.code=7;
      producer.set_value(std::move(value));
    });
    const auto started=std::chrono::steady_clock::now();
    loop.run();worker.join();
    require(observed,"loop exited before command result was posted");
    require(std::chrono::steady_clock::now()-started>=std::chrono::milliseconds(10),
      "command result keepalive did not cover worker wait");
  }
  {
    Loop loop;std::promise<ExecResult> producer;
    auto result=resultOnLoop(loop,producer.get_future().share());
    bool rejected=false;
    result.then([&](const ExecResult*,const std::exception_ptr& error){
      rejected=bool(error);
      if(error){try{std::rethrow_exception(error);}catch(const std::runtime_error& value){
        require(std::string(value.what()).empty(),"empty command error changed");}}
    });
    producer.set_exception(std::make_exception_ptr(std::runtime_error("")));
    loop.run();require(rejected,"empty command error was treated as success");
  }
  {
    std::string pattern="/tmp/batbox-async-command-XXXXXX";
    std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
    const auto created=::mkdtemp(chars.data());require(created,"mkdtemp failed");
    const std::filesystem::path root=created;
    try{
      FlightConfig config;config.shellPath="/bin/bash";
      config.originalCwd=root.string();config.outputDir=(root/"jobs").string();
      config.generateMissionId=[]{return std::string("async-1");};
      config.splitCommand=[](const std::string&){return std::vector<std::string>{"printf"};};
      auto shell=std::make_shared<ManagedShell>(std::move(config));
      auto handle=startManagedCommand(shell,RunOptions{.command="printf bridge"});
      Loop loop;bool observed=false;
      resultOnLoop(loop,handle).then([&](const ExecResult* value,const std::exception_ptr& error){
        require(loop.isOwnerThread(),"real command observer left owner strand");
        require(!error&&value&&value->code==0&&value->stdoutText.find("bridge")!=std::string::npos,
          "real shell result changed during owner bridge");
        observed=true;
      });
      loop.run();require(observed,"real command did not settle on owner loop");
      handle.cleanup();
    }catch(...){std::filesystem::remove_all(root);throw;}
    std::filesystem::remove_all(root);
  }
}
