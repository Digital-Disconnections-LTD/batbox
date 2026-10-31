#pragma once
#include "skills/Skill.hpp"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace batbox::skills {
struct SkillRosterServices {
  SkillServices skill;
  std::string managedPath,configHomeDir,homeDir,gitRoot;
  std::vector<std::string> additionalDirs;
  bool disablePolicySkills=false,userSettingsEnabled=true,
    projectSettingsEnabled=true,skillsLocked=false,bareMode=false;
  std::function<std::vector<SkillWithPath>(const std::string&)> loadLegacyCommands;
  std::function<bool(const std::string&,const std::string&)> isPathGitignored;
  std::function<bool(const std::vector<std::string>&,const std::string&)>
    ignorePatternsMatch;
  std::function<void()> clearLegacyCommandStash;
  std::function<void()> skillsLoaded;
};
struct SkillRoster {
  std::map<std::string,std::vector<SkillCommand>> stashed;
  std::set<std::string> checkedDynamicDirs,activatedConditionalNames;
  std::vector<SkillCommand> dynamicSkills,conditionalSkills;
};
std::vector<std::string> getProjectDirsUpToHome(const std::string& subdir,
  const std::string& cwd,const SkillRosterServices& services);
std::vector<SkillCommand> getSkillDirCommands(const std::string& cwd,
  SkillRoster& roster,const SkillRosterServices& services);
void clearSkillStashes(SkillRoster& roster,const SkillRosterServices& services);
std::vector<std::string> discoverSkillDirsForPaths(
  const std::vector<std::string>& filePaths,const std::string& cwd,
  SkillRoster& roster,const SkillRosterServices& services);
void addSkillDirectories(const std::vector<std::string>& dirs,
  SkillRoster& roster,const SkillRosterServices& services);
std::vector<SkillCommand> getDynamicSkills(const SkillRoster& roster);
std::vector<std::string> activateConditionalSkillsForPaths(
  const std::vector<std::string>& filePaths,const std::string& cwd,
  SkillRoster& roster,const SkillRosterServices& services);
std::size_t getConditionalSkillCount(const SkillRoster& roster);
void clearDynamicSkills(SkillRoster& roster);
}
