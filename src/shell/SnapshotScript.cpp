#include "shell/SnapshotContent.hpp"
#include "shell/ShellQuote.hpp"
#include <stdexcept>

namespace batbox::shell_snapshot {
namespace {
std::string quote(const std::string& value){return shell_quote::quote({Json(value)});}
std::string escapeAlias(std::string value){
  std::size_t pos=0;
  while((pos=value.find('\'',pos))!=std::string::npos){
    value.replace(pos,1,"'\\''");pos+=4;
  }
  return value;
}
}
std::string getBatboxSnapshotContent(const ContentServices& services){
  if(!services.pathValue)throw std::invalid_argument("Snapshot service required: pathValue");
  auto path=services.pathValue();
  if(services.windows){
    if(!services.cygwinPathValue)throw std::invalid_argument("Snapshot service required: cygwinPathValue");
    const auto cygwin=services.cygwinPathValue();
    if(cygwin&&!cygwin->empty())path=*cygwin;
  }
  const auto rg=createRipgrepShellIntegration(services);
  std::string content=R"SNAP(
      # Check for rg availability
      echo "# Check for rg availability" >> "$SNAPSHOT_FILE"
      echo "if ! (unalias rg 2>/dev/null; command -v rg) >/dev/null 2>&1; then" >> "$SNAPSHOT_FILE"
  )SNAP";
  if(rg.type=="function"){
    content+=R"SNAP(
      cat >> "$SNAPSHOT_FILE" << 'RIPGREP_FUNC_END'
  )SNAP";
    content+=rg.snippet;
    content+=R"SNAP(
RIPGREP_FUNC_END
    )SNAP";
  }else{
    const auto escaped=escapeAlias(rg.snippet);
    content+=R"SNAP(
      echo '  alias rg='"')SNAP";
    content+=escaped;
    content+=R"SNAP('" >> "$SNAPSHOT_FILE"
    )SNAP";
  }
  content+=R"SNAP(
      echo "fi" >> "$SNAPSHOT_FILE"
  )SNAP";
  const auto findGrep=createFindGrepShellIntegration(services);
  if(findGrep){
    content+=R"SNAP(
      # Shadow find/grep with embedded bfs/ugrep (Batbox only)
      echo "# Shadow find/grep with embedded bfs/ugrep" >> "$SNAPSHOT_FILE"
      cat >> "$SNAPSHOT_FILE" << 'FIND_GREP_FUNC_END'
)SNAP";
    content+=*findGrep;
    content+=R"SNAP(
FIND_GREP_FUNC_END
    )SNAP";
  }
  content+=R"SNAP(

      # Add PATH to the file
      echo "export PATH=)SNAP";
  content+=quote(path);
  content+=R"SNAP(" >> "$SNAPSHOT_FILE"
  )SNAP";
  return content;
}
std::string getSnapshotScript(const std::string& shellPath,
  const std::string& snapshotFilePath,bool configFileExists,
  const std::string& homeDirectory,const ContentServices& services){
  const auto configFile=getConfigFile(shellPath,homeDirectory);
  const bool zsh=configFile.ends_with(".zshrc");
  const auto user=configFileExists?getUserSnapshotContent(configFile):
    zsh?std::string():std::string("echo \"shopt -s expand_aliases\" >> \"$SNAPSHOT_FILE\"");
  const auto product=getBatboxSnapshotContent(services);
  std::string script="SNAPSHOT_FILE="+quote(snapshotFilePath)+"\n      "+
    (configFileExists?"source \""+configFile+"\" < /dev/null":
      "# No user config file to source");
  script+=R"SNAP(

      # First, create/clear the snapshot file
      echo "# Snapshot file" >| "$SNAPSHOT_FILE"

      # When this file is sourced, we first unalias to avoid conflicts
      # This is necessary because aliases get "frozen" inside function definitions at definition time,
      # which can cause unexpected behavior when functions use commands that conflict with aliases
      echo "# Unset all aliases to avoid conflicts with functions" >> "$SNAPSHOT_FILE"
      echo "unalias -a 2>/dev/null || true" >> "$SNAPSHOT_FILE"

      )SNAP";
  script+=user;
  script+=R"SNAP(

      )SNAP";
  script+=product;
  script+=R"SNAP(

      # Exit silently on success, only report errors
      if [ ! -f "$SNAPSHOT_FILE" ]; then
        echo "Error: Snapshot file was not created at $SNAPSHOT_FILE" >&2
        exit 1
      fi
    )SNAP";
  return script;
}
} // namespace batbox::shell_snapshot
