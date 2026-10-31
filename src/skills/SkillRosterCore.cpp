#include "skills/SkillRoster.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Unicode.hpp"
#ifdef _WIN32
#include <unicode/locid.h>
#include <unicode/unistr.h>
#endif
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::skills {
namespace {
void debug(const SkillRosterServices& services,const std::string& text){
  if(services.skill.logDebug)services.skill.logDebug(text);
}
std::string resolved(const std::string& path){
  return runtime::pathToUtf8(std::filesystem::absolute(
    runtime::pathFromUtf8(path)).lexically_normal());
}
std::string comparisonPath(const std::filesystem::path& path){
  auto value=runtime::pathToUtf8(path.lexically_normal());
#ifdef _WIN32
  std::replace(value.begin(),value.end(),'/','\\');
  const auto units=value::utf16CodeUnits(value);
  icu::UnicodeString text(reinterpret_cast<const UChar*>(units.data()),
    static_cast<int32_t>(units.size()));
  text.toLower(icu::Locale::getRoot());
  value=value::fromUtf16(std::u16string_view(
    reinterpret_cast<const char16_t*>(text.getBuffer()),text.length()));
#endif
  return value;
}
std::vector<SkillWithPath> load(const std::string& path,const std::string& source,
  const SkillRosterServices& services){
  return loadSkillsFromSkillsDir(path,source,services.skill);
}
}
std::vector<std::string> getProjectDirsUpToHome(const std::string& subdir,
  const std::string& cwd,const SkillRosterServices& services){
  const auto home=comparisonPath(runtime::pathFromUtf8(
    value::normalizeNFC(resolved(services.homeDir))));
  const auto gitRoot=services.gitRoot.empty()?std::string{}:
    comparisonPath(runtime::pathFromUtf8(resolved(services.gitRoot)));
  auto current=runtime::pathFromUtf8(resolved(cwd));
  std::vector<std::string> dirs;
  for(;;){
    if(comparisonPath(current)==home)break;
    const auto subpath=current/".batbox"/runtime::pathFromUtf8(subdir);
    std::error_code error;
    const auto status=std::filesystem::status(subpath,error);
    if(!error&&std::filesystem::exists(status))dirs.push_back(runtime::pathToUtf8(subpath));
    else if(error!=std::errc::no_such_file_or_directory&&
            error!=std::errc::not_a_directory&&
            error!=std::errc::permission_denied)
      throw std::filesystem::filesystem_error("stat",subpath,error);
    if(!gitRoot.empty()&&comparisonPath(current)==gitRoot)break;
    const auto parent=current.parent_path();
    if(parent==current)break;
    current=parent;
  }
  return dirs;
}
std::vector<SkillCommand> getSkillDirCommands(const std::string& cwd,
  SkillRoster& roster,const SkillRosterServices& services){
  if(const auto found=roster.stashed.find(cwd);found!=roster.stashed.end())
    return found->second;
  const auto userDir=runtime::pathFromUtf8(services.configHomeDir)/"skills";
  const auto managedDir=runtime::pathFromUtf8(services.managedPath)/".batbox"/"skills";
  const auto projectDirs=getProjectDirsUpToHome("skills",cwd,services);
  debug(services,"Loading skills from: managed="+runtime::pathToUtf8(managedDir)+
    ", user="+runtime::pathToUtf8(userDir));
  const bool projectEnabled=services.projectSettingsEnabled&&!services.skillsLocked;
  if(services.bareMode){
    std::vector<SkillCommand> result;
    if(!services.additionalDirs.empty()&&projectEnabled){
      for(const auto& dir:services.additionalDirs){
        for(const auto& entry:load(runtime::pathToUtf8(runtime::pathFromUtf8(dir)/".batbox"/"skills"),
          "projectSettings",services))result.push_back(entry.skill);
      }
    }
    roster.stashed[cwd]=result;
    return result;
  }
  std::vector<SkillWithPath> all;
  if(!services.disablePolicySkills){
    auto group=load(runtime::pathToUtf8(managedDir),"policySettings",services);
    all.insert(all.end(),group.begin(),group.end());
  }
  if(services.userSettingsEnabled&&!services.skillsLocked){
    auto group=load(runtime::pathToUtf8(userDir),"userSettings",services);
    all.insert(all.end(),group.begin(),group.end());
  }
  if(projectEnabled){
    for(const auto& dir:projectDirs){
      auto group=load(dir,"projectSettings",services);
      all.insert(all.end(),group.begin(),group.end());
    }
    for(const auto& dir:services.additionalDirs){
      auto group=load(runtime::pathToUtf8(runtime::pathFromUtf8(dir)/".batbox"/"skills"),
        "projectSettings",services);
      all.insert(all.end(),group.begin(),group.end());
    }
  }
  if(!services.skillsLocked){
    if(!services.loadLegacyCommands)
      throw std::invalid_argument("Missing skill binding: loadLegacyCommands");
    auto group=services.loadLegacyCommands(cwd);
    all.insert(all.end(),group.begin(),group.end());
  }
  std::set<std::string> seen;
  std::vector<SkillCommand> deduplicated;
  for(const auto& entry:all){
    const auto id=getFileIdentity(entry.filePath);
    if(id&&!seen.insert(*id).second){
      debug(services,"Skipping duplicate skill '"+entry.skill.name+"' from "+
        entry.skill.source+" (same file already loaded)");
      continue;
    }
    deduplicated.push_back(entry.skill);
  }
  std::vector<SkillCommand> unconditional;
  for(const auto& skill:deduplicated){
    if(!skill.paths.empty()&&
       !roster.activatedConditionalNames.contains(skill.name)){
      auto found=std::find_if(roster.conditionalSkills.begin(),
        roster.conditionalSkills.end(),[&](const auto& item){
          return item.name==skill.name;});
      if(found==roster.conditionalSkills.end())roster.conditionalSkills.push_back(skill);
      else *found=skill;
    }else unconditional.push_back(skill);
  }
  roster.stashed[cwd]=unconditional;
  return unconditional;
}
void clearSkillStashes(SkillRoster& roster,const SkillRosterServices& services){
  roster.stashed.clear();
  if(services.clearLegacyCommandStash)services.clearLegacyCommandStash();
  roster.conditionalSkills.clear();
  roster.activatedConditionalNames.clear();
}
}
