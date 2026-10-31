#include "shell/SnapshotContent.hpp"
#include "shell/PosixPath.hpp"
#include "shell/ShellQuote.hpp"
#include <stdexcept>

namespace batbox::shell_snapshot {
namespace {
std::string quote(const std::string& value){return shell_quote::quote({Json(value)});}
std::string join(const std::vector<std::string>& values,const std::string& separator){
  std::string result;
  for(std::size_t i=0;i<values.size();++i){if(i)result+=separator;result+=values[i];}
  return result;
}
void require(bool ready,const char* name){
  if(!ready)throw std::invalid_argument(std::string("Snapshot service required: ")+name);
}
}
std::string createArgv0ShellFunction(const std::string& funcName,
  const std::string& argv0,const std::string& binaryPath,
  const std::vector<std::string>& prependArgs){
  const auto quotedPath=quote(binaryPath);
  const auto argSuffix=prependArgs.empty()?std::string("\"$@\""):
    join(prependArgs," ")+" \"$@\"";
  return join({
    "function "+funcName+" {",
    "  if [[ -n $ZSH_VERSION ]]; then",
    "    ARGV0="+argv0+" "+quotedPath+" "+argSuffix,
    "  elif [[ \"$OSTYPE\" == \"msys\" ]] || [[ \"$OSTYPE\" == \"cygwin\" ]] || [[ \"$OSTYPE\" == \"win32\" ]]; then",
    "    ARGV0="+argv0+" "+quotedPath+" "+argSuffix,
    "  elif [[ $BASHPID != $$ ]]; then",
    "    exec -a "+argv0+" "+quotedPath+" "+argSuffix,
    "  else",
    "    (exec -a "+argv0+" "+quotedPath+" "+argSuffix+")",
    "  fi",
    "}"
  },"\n");
}
Integration createRipgrepShellIntegration(const ContentServices& services){
  require(bool(services.ripgrepCommand),"ripgrepCommand");
  const auto rg=services.ripgrepCommand();
  if(rg.argv0)return {"function",createArgv0ShellFunction("rg",*rg.argv0,rg.path)};
  std::vector<std::string> quotedArgs;
  for(const auto& arg:rg.args)quotedArgs.push_back(quote(arg));
  const auto quotedPath=quote(rg.path);
  const auto target=quotedArgs.empty()?quotedPath:quotedPath+" "+join(quotedArgs," ");
  return {"alias",target};
}
std::optional<std::string> createFindGrepShellIntegration(const ContentServices& services){
  require(bool(services.hasEmbeddedSearchGadgets),"hasEmbeddedSearchTools");
  if(!services.hasEmbeddedSearchGadgets())return std::nullopt;
  require(bool(services.embeddedSearchGadgetsBinaryPath),"embeddedSearchToolsBinaryPath");
  const auto binary=services.embeddedSearchGadgetsBinaryPath();
  std::vector<std::string> grepArgs={"-G","--ignore-files","--hidden","-I"};
  for(const auto& dir:{".git",".svn",".hg",".bzr",".jj",".sl"})
    grepArgs.push_back(std::string("--exclude-dir=")+dir);
  return join({
    "unalias find 2>/dev/null || true",
    "unalias grep 2>/dev/null || true",
    createArgv0ShellFunction("find","bfs",binary,{"-regextype","findutils-default"}),
    createArgv0ShellFunction("grep","ugrep",binary,grepArgs)
  },"\n");
}
std::string getConfigFile(const std::string& shellPath,const std::string& homeDirectory){
  const auto fileName=shellPath.find("zsh")!=std::string::npos?".zshrc":
    shellPath.find("bash")!=std::string::npos?".bashrc":".profile";
  return shell::joinPosixPath(homeDirectory,fileName);
}
} // namespace batbox::shell_snapshot
