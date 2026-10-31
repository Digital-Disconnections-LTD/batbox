#include "shell/ShellBootstrap.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace batbox;
int main(){
  namespace fs=std::filesystem;
  const auto dir=fs::temp_directory_path()/("batbox-shell-discovery-"+std::to_string(::getpid()));
  fs::create_directories(dir);
  const auto binary=dir/"bash";
  {std::ofstream out(binary);out<<"#!/bin/sh\nexit 0\n";}
  fs::permissions(binary,fs::perms::owner_read|fs::perms::owner_exec);
  assert(shell::indigenousIsExecutable(binary.string()));
  assert(!shell::indigenousIsExecutable((dir/"missing").string()));
  event_loop::Loop loop;
  auto which=shell::indigenousWhich(loop,dir.string());
  auto found=which("bash");std::optional<std::string> path;
  found.then([&](const std::optional<std::string>* value,const std::exception_ptr& error){
    assert(!error);path=*value;
  });
  loop.runReady();assert(path&&*path==binary.string());
  shell::DiscoveryServices services;
  services.shellOverride=binary.string();
  services.isExecutable=shell::indigenousIsExecutable;
  services.which=which;
  services.debug=[](const std::string&){};
  services.error=[](const std::string&){assert(false);};
  auto selected=shell::findSuitableShellAsync(loop,services);
  std::string value;
  selected.then([&](const std::string* result,const std::exception_ptr& error){
    assert(!error);value=*result;
  });
  loop.runReady();assert(value==binary.string());
  fs::remove_all(dir);
}
