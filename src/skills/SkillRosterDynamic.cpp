#include "skills/SkillRoster.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::skills {
namespace {
constexpr char pathSeparator=static_cast<char>(std::filesystem::path::preferred_separator);
void upsert(std::vector<SkillCommand>& skills,const SkillCommand& skill){
  const auto it=std::find_if(skills.begin(),skills.end(),[&](const auto& item){
    return item.name==skill.name;});
  if(it==skills.end())skills.push_back(skill);
  else *it=skill;
}
std::size_t depth(const std::string& path){
  return std::count(path.begin(),path.end(),pathSeparator)+1;
}
}
std::vector<std::string> discoverSkillDirsForPaths(
  const std::vector<std::string>& filePaths,const std::string& cwd,
  SkillRoster& roster,const SkillRosterServices& services){
  std::string resolvedCwd=cwd;
  if(!resolvedCwd.empty()&&resolvedCwd.back()==pathSeparator)resolvedCwd.pop_back();
  std::vector<std::string> newDirs;
  for(const auto& filePath:filePaths){
    auto current=runtime::pathFromUtf8(filePath).parent_path();
    while(runtime::pathToUtf8(current).starts_with(resolvedCwd+pathSeparator)){
      const auto skillDir=runtime::pathToUtf8(current/".batbox"/"skills");
      if(roster.checkedDynamicDirs.insert(skillDir).second){
        std::error_code error;
        const auto status=std::filesystem::status(runtime::pathFromUtf8(skillDir),error);
        if(!error&&std::filesystem::exists(status)){
          if(!services.isPathGitignored)
            throw std::invalid_argument("Missing skill binding: isPathGitignored");
          if(!services.isPathGitignored(runtime::pathToUtf8(current),resolvedCwd))
            newDirs.push_back(skillDir);
        }
      }
      const auto parent=current.parent_path();
      if(parent==current)break;
      current=parent;
    }
  }
  std::stable_sort(newDirs.begin(),newDirs.end(),[](const auto& a,const auto& b){
    return depth(a)>depth(b);
  });
  return newDirs;
}
void addSkillDirectories(const std::vector<std::string>& dirs,
  SkillRoster& roster,const SkillRosterServices& services){
  if(!services.projectSettingsEnabled||services.skillsLocked||dirs.empty())return;
  std::vector<std::vector<SkillWithPath>> loaded;
  for(const auto& dir:dirs)loaded.push_back(loadSkillsFromSkillsDir(
    dir,"projectSettings",services.skill));
  for(auto it=loaded.rbegin();it!=loaded.rend();++it)
    for(const auto& entry:*it)upsert(roster.dynamicSkills,entry.skill);
  if(services.skillsLoaded)services.skillsLoaded();
}
std::vector<SkillCommand> getDynamicSkills(const SkillRoster& roster){
  return roster.dynamicSkills;
}
std::vector<std::string> activateConditionalSkillsForPaths(
  const std::vector<std::string>& filePaths,const std::string& cwd,
  SkillRoster& roster,const SkillRosterServices& services){
  if(roster.conditionalSkills.empty())return {};
  if(!services.ignorePatternsMatch)
    throw std::invalid_argument("Missing skill binding: ignorePatternsMatch");
  std::vector<std::string> activated;
  for(std::size_t i=0;i<roster.conditionalSkills.size();){
    const auto& skill=roster.conditionalSkills[i];
    if(skill.paths.empty()){++i;continue;}
    bool matched=false;
    for(const auto& filePath:filePaths){
      std::string relativePath=filePath;
      if(runtime::pathFromUtf8(filePath).is_absolute())
        relativePath=runtime::pathToUtf8(runtime::pathFromUtf8(filePath).
          lexically_relative(runtime::pathFromUtf8(cwd)));
      if(relativePath.empty()||relativePath.starts_with("..")||
         runtime::pathFromUtf8(relativePath).is_absolute())continue;
      if(services.ignorePatternsMatch(skill.paths,relativePath)){
        upsert(roster.dynamicSkills,skill);
        roster.activatedConditionalNames.insert(skill.name);
        activated.push_back(skill.name);
        matched=true;
        break;
      }
    }
    if(matched)roster.conditionalSkills.erase(roster.conditionalSkills.begin()+
      static_cast<std::ptrdiff_t>(i));
    else ++i;
  }
  if(!activated.empty()&&services.skillsLoaded)services.skillsLoaded();
  return activated;
}
std::size_t getConditionalSkillCount(const SkillRoster& roster){
  return roster.conditionalSkills.size();
}
void clearDynamicSkills(SkillRoster& roster){
  roster.checkedDynamicDirs.clear();
  roster.dynamicSkills.clear();
  roster.conditionalSkills.clear();
  roster.activatedConditionalNames.clear();
}
}
