#include "instructions/MemoryPoetry.hpp"
#include "instructions/MemoryTypes.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "shell/PosixPath.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace batbox::instructions {
namespace {
void append(std::vector<std::string>& out,const std::vector<std::string_view>& source){
  for(const auto item:source)out.emplace_back(item);
}
std::string joinLines(const std::vector<std::string>& lines){
  std::string result;
  for(std::size_t i=0;i<lines.size();++i){if(i)result+='\n';result+=lines[i];}
  return result;
}
std::string readUtf8(const std::string& file){
  std::ifstream input(runtime::pathFromUtf8(file),std::ios::binary);
  if(!input)throw std::runtime_error("Cannot read memory entrypoint");
  std::string bytes((std::istreambuf_iterator<char>(input)),{});
  return value::decodeUtf8(bytes,false);
}
}
void ensureMemoryDirExists(const std::string& memoryDir,const MemoryPoetryServices& services){
  std::error_code error;
  std::filesystem::create_directories(runtime::pathFromUtf8(memoryDir),error);
  if(error&&services.logDebug)
    services.logDebug("ensureMemoryDirExists failed for "+memoryDir+": "+error.message());
}
std::string buildMemoryPoetry(const std::string& displayName,const std::string& memoryDir,
  const std::optional<std::vector<std::string>>& extraGuidelines,
  const MemoryPoetryServices& services){
  const auto entrypoint=memoryDir+"MEMORY.md";
  std::string entrypointContent;
  try{entrypointContent=readUtf8(entrypoint);}catch(const std::exception&){}
  auto lines=buildMemoryLines(displayName,memoryDir,extraGuidelines,false,services);
  if(!value::trim(entrypointContent).empty()){
    const auto truncated=truncateEntrypointContent(entrypointContent);
    lines.insert(lines.end(),{"## MEMORY.md","",truncated.content});
  }else lines.insert(lines.end(),{"## MEMORY.md","",
    "Your MEMORY.md is currently empty. When you save new memories, they will appear here."});
  return joinLines(lines);
}
std::string buildAssistantDailyLogPoetry(bool skipIndex,const MemoryPoetryServices& services,
  MemoryPathStash& stash){
  const auto memoryDir=getAutoMemPath(services.paths,stash);
  const auto pattern=shell::joinPosixPath(shell::joinPosixPath(shell::joinPosixPath(
    shell::joinPosixPath(memoryDir,"logs"),"YYYY"),"MM"),"YYYY-MM-DD.md");
  std::vector<std::string> lines={"# auto memory","",
    "You have a persistent, file-based memory system found at: `"+memoryDir+"`","",
    "This session is long-lived. As you work, record anything worth remembering by **appending** to today's daily log file:",
    "","`"+pattern+"`","",
    "Substitute today's date (from `currentDate` in your context) for `YYYY-MM-DD`. When the date rolls over mid-session, start appending to the new day's file.",
    "",
    "Write each entry as a short timestamped bullet. Create the file (and parent directories) on first write if it does not exist. Do not rewrite or reorganize the log — it is append-only. A separate nightly process distills these logs into `MEMORY.md` and topic files.",
    "","## What to log",
    "- User corrections and preferences (\"use bun, not npm\"; \"stop summarizing diffs\")",
    "- Facts about the user, their role, or their goals",
    "- Project context that is not derivable from the code (deadlines, incidents, decisions and their rationale)",
    "- Pointers to external systems (dashboards, Linear projects, Slack channels)",
    "- Anything the user explicitly asks you to remember",""};
  append(lines,memoryWhatNotToSaveSection());
  lines.push_back("");
  if(!skipIndex)lines.insert(lines.end(),{"## MEMORY.md",
    "`MEMORY.md` is the distilled index (maintained nightly from your logs) and is loaded into your context automatically. Read it for orientation, but do not edit it directly — record new information in today's log instead.",
    ""});
  const auto search=buildSearchingPastContextSection(memoryDir,services);
  lines.insert(lines.end(),search.begin(),search.end());
  return joinLines(lines);
}
std::optional<std::string> loadMemoryPoetry(const MemoryPoetryServices& services,
  MemoryPathStash& stash){
  const bool autoEnabled=isAutoMemoryEnabled(services.paths);
  if(services.dailyLogFeature&&autoEnabled&&services.kairosActive)
    return buildAssistantDailyLogPoetry(services.skipIndex,services,stash);
  std::optional<std::vector<std::string>> guidelines;
  if(services.extraGuidelines&&!value::trim(*services.extraGuidelines).empty())
    guidelines=std::vector<std::string>{*services.extraGuidelines};
  if(services.teamMemoryFeature&&services.teamMemoryEnabled){
    if(!services.getTeamMemPath||!services.buildCombinedMemoryPoetry)
      throw std::invalid_argument("Missing team memory path or prompt import");
    const auto teamDir=services.getTeamMemPath();
    ensureMemoryDirExists(teamDir,services);
    return services.buildCombinedMemoryPoetry(guidelines,services.skipIndex);
  }
  if(autoEnabled){
    const auto autoDir=getAutoMemPath(services.paths,stash);
    ensureMemoryDirExists(autoDir,services);
    return joinLines(buildMemoryLines("auto memory",autoDir,guidelines,
      services.skipIndex,services));
  }
  return std::nullopt;
}
} // namespace batbox::instructions
