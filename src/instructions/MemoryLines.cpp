#include "instructions/MemoryPoetry.hpp"
#include "instructions/MemoryTypes.hpp"
#include <stdexcept>

namespace batbox::instructions {
namespace {
void append(std::vector<std::string>& out,const std::vector<std::string_view>& source){
  for(const auto item:source)out.emplace_back(item);
}
void append(std::vector<std::string>& out,const std::optional<std::vector<std::string>>& source){
  if(source)out.insert(out.end(),source->begin(),source->end());
}
}
std::vector<std::string> buildSearchingPastContextSection(const std::string& autoMemDir,
  const MemoryPoetryServices& services){
  if(!services.searchPastContextEnabled)return {};
  if(!services.getProjectDir)throw std::invalid_argument("Missing project directory import");
  const auto projectDir=services.getProjectDir(services.originalCwd);
  const bool embedded=services.embeddedSearchGadgets||services.replMode;
  const auto memSearch=embedded
    ?"grep -rn \"<search term>\" "+autoMemDir+" --include=\"*.md\""
    :services.grepGadgetName+" with pattern=\"<search term>\" path=\""+
      autoMemDir+"\" glob=\"*.md\"";
  const auto transcriptSearch=embedded
    ?"grep -rn \"<search term>\" "+projectDir+"/ --include=\"*.jsonl\""
    :services.grepGadgetName+" with pattern=\"<search term>\" path=\""+
      projectDir+"/\" glob=\"*.jsonl\"";
  return {"## Searching past context","","When looking for past context:",
    "1. Search topic files in your memory directory:","```",memSearch,"```",
    "2. Session transcript logs (last resort — large files, slow):",
    "```",transcriptSearch,"```",
    "Use narrow search terms (error messages, file paths, function names) rather than broad keywords.",
    ""};
}
std::vector<std::string> buildMemoryLines(const std::string& displayName,
  const std::string& memoryDir,const std::optional<std::vector<std::string>>& extraGuidelines,
  bool skipIndex,const MemoryPoetryServices& services){
  std::vector<std::string> howToSave;
  if(skipIndex){
    howToSave={"## How to save memories","",
      "Write each memory to its own file (e.g., `user_role.md`, `feedback_testing.md`) using this frontmatter format:",""};
    append(howToSave,memoryMemoryFrontmatterExample());
    howToSave.insert(howToSave.end(),{"",
      "- Keep the name, description, and type fields in memory files up-to-date with the content",
      "- Organize memory semantically by topic, not chronologically",
      "- Update or remove memories that turn out to be wrong or outdated",
      "- Do not write duplicate memories. First check if there is an existing memory you can update before writing a new one."});
  }else{
    howToSave={"## How to save memories","","Saving a memory is a two-step process:","",
      "**Step 1** — write the memory to its own file (e.g., `user_role.md`, `feedback_testing.md`) using this frontmatter format:",""};
    append(howToSave,memoryMemoryFrontmatterExample());
    howToSave.insert(howToSave.end(),{"",
      "**Step 2** — add a pointer to that file in `MEMORY.md`. `MEMORY.md` is an index, not a memory — each entry should be one line, under ~150 characters: `- [Title](file.md) — one-line hook`. It has no frontmatter. Never write memory content directly into `MEMORY.md`.",
      "","- `MEMORY.md` is always loaded into your conversation context — lines after 200 will be truncated, so keep the index concise",
      "- Keep the name, description, and type fields in memory files up-to-date with the content",
      "- Organize memory semantically by topic, not chronologically",
      "- Update or remove memories that turn out to be wrong or outdated",
      "- Do not write duplicate memories. First check if there is an existing memory you can update before writing a new one."});
  }
  std::vector<std::string> lines={"# "+displayName,"",
    "You have a persistent, file-based memory system at `"+memoryDir+
      "`. This directory already exists — write to it directly with the Write tool (do not run mkdir or check for its existence).",
    "",
    "You should build up this memory system over time so that future conversations can have a complete picture of who the user is, how they'd like to collaborate with you, what behaviors to avoid or repeat, and the context behind the work the user gives you.",
    "",
    "If the user explicitly asks you to remember something, save it immediately as whichever type fits best. If they ask you to forget something, find and remove the relevant entry.",
    ""};
  append(lines,memoryTypesSectionIndividual());
  append(lines,memoryWhatNotToSaveSection());
  lines.push_back("");
  lines.insert(lines.end(),howToSave.begin(),howToSave.end());
  lines.push_back("");
  append(lines,memoryWhenToAccessSection());
  lines.push_back("");
  append(lines,memoryTrustingRecallSection());
  lines.insert(lines.end(),{"","## Memory and other forms of persistence",
    "Memory is one of several persistence mechanisms available to you as you assist the user in a given conversation. The distinction is often that memory can be recalled in future conversations and should not be used for persisting information that is only useful within the scope of the current conversation.",
    "- When to use or update a plan instead of memory: If you are about to start a non-trivial implementation task and would like to reach alignment with the user on your approach you should use a Plan rather than saving this information to memory. Similarly, if you already have a plan within the conversation and you have changed your approach persist that change by updating the plan rather than saving a memory.",
    "- When to use or update tasks instead of memory: When you need to break your work in current conversation into discrete steps or keep track of your progress use tasks instead of saving to memory. Tasks are great for persisting information about the work that needs to be done in the current conversation, but memory should be reserved for information that will be useful in future conversations.",
    ""});
  append(lines,extraGuidelines);
  lines.push_back("");
  const auto search=buildSearchingPastContextSection(memoryDir,services);
  lines.insert(lines.end(),search.begin(),search.end());
  return lines;
}
} // namespace batbox::instructions
