#include "application_skills/Provider.hpp"
#include "instructions/Frontmatter.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

namespace batbox::application_skills {
namespace {
using Path=std::filesystem::path;
std::string commandName(const Path& file,const Path& base){
  Path relative=file.lexically_relative(base);
  std::vector<std::string> names;
  for(const auto& part:relative.parent_path())names.push_back(
    runtime::pathToUtf8(part));
  if(file.filename()=="SKILL.md"){
    if(names.empty())return {};
    const auto name=names.back();names.pop_back();names.push_back(name);
  }else names.push_back(runtime::pathToUtf8(file.stem()));
  std::string result;
  for(const auto& name:names){if(!result.empty())result+=':';result+=name;}
  return result;
}
void collect(const Path& base,const std::string& source,
    const skills::SkillRosterServices& services,
    std::set<Path>& seen,std::vector<skills::SkillWithPath>& rows){
  std::error_code error;
  if(!std::filesystem::is_directory(base,error))return;
  std::vector<Path> files;
  std::filesystem::recursive_directory_iterator it(base,
    std::filesystem::directory_options::follow_directory_symlink|
      std::filesystem::directory_options::skip_permission_denied,error),end;
  for(;it!=end;it.increment(error)){
    if(error){if(services.skill.logError)services.skill.logError(error.message());
      error.clear();continue;}
    if(it->path().extension()==".md"&&it->is_regular_file(error))
      files.push_back(it->path());
    error.clear();
  }
  std::sort(files.begin(),files.end());
  for(const auto& file:files){
    const auto canonical=std::filesystem::canonical(file,error);
    if(!error&&!seen.insert(canonical).second)continue;
    error.clear();
    try{
      std::ifstream stream(file,std::ios::binary);
      if(!stream)throw std::runtime_error("Cannot read legacy command");
      const std::string markdown(std::istreambuf_iterator<char>(stream),{});
      const auto parsed=instructions::parseFrontmatter(markdown,
        runtime::pathToUtf8(file),services.skill.logDebug);
      const auto name=commandName(file,base);
      if(name.empty())continue;
      const auto fields=skills::parseSkillFrontmatterFields(parsed.frontmatter,
        parsed.content,name,"Custom command",services.skill);
      const std::optional<std::string> skillDir=file.filename()=="SKILL.md"
        ?std::optional<std::string>(runtime::pathToUtf8(file.parent_path()))
        :std::nullopt;
      rows.push_back({skills::createSkillCommand(fields,name,parsed.content,
        source,skillDir,"commands_DEPRECATED",{}),runtime::pathToUtf8(file)});
    }catch(const std::exception& failure){
      if(services.skill.logError)services.skill.logError(
        "Legacy command "+runtime::pathToUtf8(file)+": "+failure.what());
    }
  }
}
}
std::vector<skills::SkillWithPath> Provider::loadLegacy(const std::string& cwd){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Legacy skill discovery requires owner loop");
  std::vector<skills::SkillWithPath> rows;
  std::set<Path> seen;
  if(!services_.managedPath.empty())collect(runtime::pathFromUtf8(services_.managedPath)/
    ".batbox"/"commands","policySettings",services_,seen,rows);
  if(services_.userSettingsEnabled)
    collect(runtime::pathFromUtf8(services_.configHomeDir)/"commands",
      "userSettings",services_,seen,rows);
  if(services_.projectSettingsEnabled){
    for(const auto& dir:skills::getProjectDirsUpToHome(
        "commands",cwd,services_))
      collect(runtime::pathFromUtf8(dir),"projectSettings",services_,seen,rows);
    for(const auto& dir:services_.additionalDirs)
      collect(runtime::pathFromUtf8(dir)/".batbox"/"commands",
        "projectSettings",services_,seen,rows);
  }
  return rows;
}
}
