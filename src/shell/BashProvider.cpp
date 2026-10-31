#include "shell/BashProvider.hpp"
#include "shell/CommandQuoting.hpp"
#include "shell/PipeCommand.hpp"
#include "shell/PosixPath.hpp"
#include "shell/ShellQuote.hpp"
#include <stdexcept>

namespace batbox::shell_provider {
namespace {
std::string sourcePath(const std::string& path){return shell_quote::quote({Json(path)});}
std::string joinParts(const std::vector<std::string>& parts){
  std::string result;
  for(const auto& part:parts){if(!result.empty())result+=" && ";result+=part;}
  return result;
}
void require(bool ready,const char* name){
  if(!ready)throw std::invalid_argument(std::string("Bash provider service required: ")+name);
}
}
std::optional<std::string> getDisableExtglobCommand(const std::string& shellPath,
  bool shellPrefixConfigured){
  if(shellPrefixConfigured)
    return "{ shopt -u extglob || setopt NO_EXTENDED_GLOB; } >/dev/null 2>&1 || true";
  if(shellPath.find("bash")!=std::string::npos)return "shopt -u extglob 2>/dev/null || true";
  if(shellPath.find("zsh")!=std::string::npos)return "setopt NO_EXTENDED_GLOB 2>/dev/null || true";
  return std::nullopt;
}
BashProvider::BashProvider(std::string shellPath,Services services):
  shellPath_(std::move(shellPath)),services_(std::move(services)){
  require(!shellPath_.empty(),"shellPath");
  if(!services_.skipSnapshot)require(bool(services_.createSnapshot),"createSnapshot");
  require(bool(services_.flightEnvironmentScript),"sessionEnvironmentScript");
  require(bool(services_.flightEnvVars),"sessionEnvVars");
  require(bool(services_.tmuxGadgetUsed),"tmuxToolUsed");
  require(bool(services_.ensureTmuxSocket),"ensureTmuxSocket");
  require(bool(services_.tmuxEnvironment),"tmuxEnvironment");
  require(bool(services_.operatingSystemTempDir),"operatingSystemTempDir");
  require(bool(services_.snapshotAccessible),"snapshotAccessible");
  if(services_.windows){
    require(bool(services_.indigenousToPosixPath),"nativeToPosixPath");
    require(bool(services_.indigenousJoin),"nativeJoin");
  }
  if(!services_.skipSnapshot)snapshotFuture_=services_.createSnapshot(shellPath_);
}
BuildResult BashProvider::buildExecCommand(const std::string& command,const BuildOptions& options){
  std::optional<std::string> snapshot;
  if(!services_.skipSnapshot)try{snapshot=snapshotFuture_.get();}catch(...){snapshot.reset();}
  if(snapshot)try{
    if(!services_.snapshotAccessible(*snapshot))snapshot.reset();
  }catch(...){snapshot.reset();}
  {
    std::lock_guard lock(mutex_);
    lastSnapshotPath_=snapshot;
    currentSandboxTmpDir_=options.sandboxTmpDir;
  }
  const auto tmpdir=services_.operatingSystemTempDir();
  const auto shellTmpdir=services_.windows?services_.indigenousToPosixPath(tmpdir):tmpdir;
  const auto shellCwdFilePath=options.useSandbox?
    shell::joinPosixPath(options.sandboxTmpDir.value(),"cwd-"+options.id):
    shell::joinPosixPath(shellTmpdir,"batbox-"+options.id+"-cwd");
  const auto cwdFilePath=options.useSandbox?
    shell::joinPosixPath(options.sandboxTmpDir.value(),"cwd-"+options.id):
    services_.windows?services_.indigenousJoin(tmpdir,"batbox-"+options.id+"-cwd"):
      shell::joinPosixPath(tmpdir,"batbox-"+options.id+"-cwd");
  const auto normalized=shell_command::rewriteWindowsNullRedirect(command);
  const bool addRedirect=shell_command::shouldAddStdinRedirect(normalized);
  auto quoted=shell_command::quoteShellCommand(normalized,addRedirect);
  if(normalized.find('|')!=std::string::npos&&addRedirect)
    quoted=shell_command::rearrangePipeCommand(normalized);
  std::vector<std::string> parts;
  if(snapshot){
    const auto path=services_.windows?services_.indigenousToPosixPath(*snapshot):*snapshot;
    parts.push_back("source "+sourcePath(path)+" 2>/dev/null || true");
  }
  const auto script=services_.flightEnvironmentScript();
  if(script&&!script->empty())parts.push_back(*script);
  const auto disable=getDisableExtglobCommand(shellPath_,!services_.shellPrefix.empty());
  if(disable)parts.push_back(*disable);
  parts.push_back("eval "+quoted);
  parts.push_back("pwd -P >| "+sourcePath(shellCwdFilePath));
  auto commandString=joinParts(parts);
  if(!services_.shellPrefix.empty())
    commandString=shell_command::formatShellPrefixCommand(services_.shellPrefix,commandString);
  return {commandString,cwdFilePath};
}
std::vector<std::string> BashProvider::getSpawnArgs(const std::string& commandString)const{
  std::lock_guard lock(mutex_);
  if(lastSnapshotPath_)return {"-c",commandString};
  return {"-c","-l",commandString};
}
std::map<std::string,std::string> BashProvider::getEnvironmentOverrides(const std::string& command){
  if(services_.internalOperator&&(services_.tmuxGadgetUsed()||
    command.find("tmux")!=std::string::npos))services_.ensureTmuxSocket();
  const auto tmux=services_.tmuxEnvironment();
  std::map<std::string,std::string> env;
  if(tmux&&!tmux->empty())env["TMUX"]=*tmux;
  std::optional<std::string> sandboxTmpDir;
  {std::lock_guard lock(mutex_);sandboxTmpDir=currentSandboxTmpDir_;}
  if(sandboxTmpDir){
    const auto posix=services_.windows?services_.indigenousToPosixPath(*sandboxTmpDir):*sandboxTmpDir;
    env["TMPDIR"]=posix;env["BATBOX_TMPDIR"]=posix;
    env["TMPPREFIX"]=shell::joinPosixPath(posix,"zsh");
  }
  for(const auto& [key,value]:services_.flightEnvVars())env[key]=value;
  return env;
}
} // namespace batbox::shell_provider
