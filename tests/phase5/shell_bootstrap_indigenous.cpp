#include "shell/ShellBootstrap.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
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
  const auto other=dir/"fish";
  {std::ofstream out(other);out<<"#!/bin/sh\nexit 0\n";}
  fs::permissions(other,fs::perms::owner_read|fs::perms::owner_exec);
  assert(shell::indigenousIsExecutable(binary.string()));
  assert(!shell::indigenousIsExecutable((dir/"missing").string()));
  auto which=shell::indigenousWhich(dir.string());
  assert(which("bash")==binary.string());
  assert(!which("zsh"));
  shell::DiscoveryServices services;
  // Only the fixture folder counts, so the system's own shells stay out of it.
  services.isExecutable=[root=dir.string()](const std::string& path){
    return path.starts_with(root)&&shell::indigenousIsExecutable(path);};
  services.which=which;
  services.debug=[](const std::string&){};
  services.shellOverride=binary.string();
  assert(shell::findSuitableShell(services)==binary.string());
  // A SHELL that is neither bash nor zsh is passed over.
  services.shellOverride.reset();services.preferredShell=other.string();
  assert(shell::findSuitableShell(services)==binary.string());
  // So is an override that is not bash or zsh.
  services.shellOverride=other.string();
  assert(shell::findSuitableShell(services)==binary.string());
  // With no bash or zsh anywhere, /bin/sh runs the commands.
  services.shellOverride.reset();services.preferredShell.reset();
  services.which=[](const std::string&)->std::optional<std::string>{return std::nullopt;};
  services.isExecutable=[](const std::string& path){return path=="/bin/sh";};
  assert(shell::findSuitableShell(services)=="/bin/sh");
  services.isExecutable=[](const std::string&){return false;};
  bool threw=false;
  try{(void)shell::findSuitableShell(services);}catch(const std::runtime_error&){threw=true;}
  assert(threw);
  fs::remove_all(dir);
}
