#include <cstdlib>
#include "shell/CommandHandle.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace batbox;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  char temp[]="/tmp/batbox-command-handle-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  try{
    std::atomic<int> ids{0};
    shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
    config.outputDir=(root/"jobs").string();config.generateMissionId=[&]{return "task"+std::to_string(++ids);};
    config.splitCommand=[](const std::string&){return std::vector<std::string>{"echo"};};
    auto managed=std::make_shared<shell::ManagedShell>(config);
    auto quick=shell::startManagedCommand(managed,shell::RunOptions{.command="sleep 0.1; echo quick",
      .timeoutMs=2000});
    check(quick.status()==shell::CommandStatus::Running,"quick command not running after start");
    check(quick.result().get().stdoutText.find("quick")!=std::string::npos,
      "quick result did not resolve after exit");
    check(quick.status()==shell::CommandStatus::Completed,"quick status not completed");
    auto manual=shell::startManagedCommand(managed,shell::RunOptions{.command="sleep 0.3; echo manual",
      .timeoutMs=2000});
    check(!manual.background("wrong"),"mismatched task ID was accepted");
    check(manual.background(manual.missionId()),"manual background failed");
    check(manual.status()==shell::CommandStatus::Backgrounded,"manual background status wrong");
    check(manual.result().wait_for(std::chrono::milliseconds(50))==std::future_status::timeout,
      "background result resolved before process exit");
    auto manualResult=manual.result().get();
    check(manualResult.backgroundMissionId==manual.missionId()&&
      manualResult.stdoutText.find("manual")!=std::string::npos,
      "background result did not preserve ID/output");
    manual.cleanup();
    auto timeoutBackground=shell::startManagedCommand(managed,shell::RunOptions{
      .command="sleep 0.35; echo timeout background",.timeoutMs=50});
    std::atomic_bool timeoutCalled{false};
    timeoutBackground.onTimeout([&](const auto& backgroundFn){
      timeoutCalled=true;check(backgroundFn(timeoutBackground.missionId()),"timeout background failed");});
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    check(timeoutCalled&&timeoutBackground.status()==shell::CommandStatus::Backgrounded,
      "timeout did not call background handler");
    check(timeoutBackground.result().wait_for(std::chrono::milliseconds(20))==std::future_status::timeout,
      "timeout background result resolved before exit");
    check(timeoutBackground.result().get().backgroundMissionId==timeoutBackground.missionId(),
      "timeout background result missing ID");
    timeoutBackground.cleanup();
    auto timedOut=shell::startManagedCommand(managed,shell::RunOptions{
      .command="sleep 20",.timeoutMs=50});
    const auto timedOutResult=timedOut.result().get();
    check(timedOutResult.code==143&&!timedOutResult.interrupted&&
      timedOut.status()==shell::CommandStatus::Killed,"unhandled timeout did not kill with the expected exit code");
    auto killed=shell::startManagedCommand(managed,shell::RunOptions{
      .command="sleep 20",.timeoutMs=2000});
    killed.kill();
    check(killed.result().get().code==137&&killed.status()==shell::CommandStatus::Killed,
      "kill did not resolve the expected interrupt code");
    auto throwing=shell::startManagedCommand(managed,shell::RunOptions{
      .command="sleep 20",.timeoutMs=50});
    throwing.onTimeout([](const auto&){throw std::runtime_error("timeout callback failed");});
    bool caught=false;
    try{(void)throwing.result().get();}
    catch(const std::runtime_error& error){caught=std::string(error.what())=="timeout callback failed";}
    check(caught&&throwing.status()==shell::CommandStatus::Killed,
      "throwing timeout callback was not surfaced and child stopped");
    std::filesystem::remove_all(root);
    std::cout<<"PASS managed shell command handle status/background/timeout/kill\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
