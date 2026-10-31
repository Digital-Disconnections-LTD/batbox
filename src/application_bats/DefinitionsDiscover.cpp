#include "application_bats/Definitions.hpp"
#include "application_bats/BuiltinDefinitions.hpp"
#include "git_filesystem/RootFinder.hpp"
#include "runtime/IndigenousPath.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

namespace batbox::application_bats {
namespace {
using Path=std::filesystem::path;
struct File {Path path,base;std::string source;};
void note(const DefinitionInputs& inputs,const std::string& value){
  if(inputs.diagnostic)inputs.diagnostic(value);
}
std::vector<Path> projectDirs(const DefinitionInputs& inputs){
  auto finder=git_filesystem::indigenousRootFinder();
  const auto gitRoot=finder->find(runtime::pathToUtf8(inputs.config.cwd));
  const auto root=gitRoot?Path(*gitRoot):Path{};
#ifdef _WIN32
  auto home=runtime::environmentUtf8("USERPROFILE");
  if(home.empty())home=runtime::environmentUtf8("HOME");
#else
  const auto* value=std::getenv("HOME");
  if(!value||!*value)value=std::getenv("USERPROFILE");
  const std::string home=value?value:"";
#endif
  const auto homePath=runtime::pathFromUtf8(home);
  std::vector<Path> result;
  for(auto current=std::filesystem::absolute(inputs.config.cwd);;){
    if(!homePath.empty()&&current==homePath)break;
    auto candidate=current/".batbox"/"agents";
    std::error_code error;
    if(std::filesystem::is_directory(candidate,error))result.push_back(candidate);
    if(!root.empty()&&current==root)break;
    auto parent=current.parent_path();if(parent==current)break;
    current=std::move(parent);
  }
  if(!root.empty()&&!inputs.projectRoot.empty()&&
      root.lexically_normal()!=inputs.projectRoot.lexically_normal()){
    const auto worktree=root/".batbox"/"agents";
    if(std::find(result.begin(),result.end(),worktree)==result.end()){
      const auto canonical=inputs.projectRoot/".batbox"/"agents";
      if(std::find(result.begin(),result.end(),canonical)==result.end())
        result.push_back(canonical);
    }
  }
  return result;
}
std::vector<File> filesIn(const Path& dir,const std::string& source,
    const DefinitionInputs& inputs){
  std::vector<File> result;
  std::error_code error;
  if(!std::filesystem::is_directory(dir,error))return result;
  std::set<Path> seenDirectories;
  std::filesystem::recursive_directory_iterator it(dir,
    std::filesystem::directory_options::follow_directory_symlink|
    std::filesystem::directory_options::skip_permission_denied,error),end;
  if(error){note(inputs,"Agent directory scan failed: "+error.message());return result;}
  for(;it!=end;it.increment(error)){
    if(error){note(inputs,"Agent directory scan failed: "+error.message());
      error.clear();continue;}
    const auto path=it->path();
    if(it->is_directory(error)){
      const auto canonical=std::filesystem::weakly_canonical(path,error);
      if(!error&&!seenDirectories.insert(canonical).second)
        it.disable_recursion_pending();
      error.clear();continue;
    }
    error.clear();
    if(path.extension()==".md"&&it->is_regular_file(error))
      result.push_back({path,dir,source});
    error.clear();
  }
  std::sort(result.begin(),result.end(),[](const File& a,const File& b){
    return a.path<b.path;});
  return result;
}
std::vector<File> loadFiles(const DefinitionInputs& inputs){
  std::vector<File> rows;
#ifdef _WIN32
  const auto programData=std::getenv("PROGRAMDATA");
  const auto managed=programData?Path(programData)/"Batbox":Path{};
#else
  const Path managed="/etc/batbox";
#endif
  if(!managed.empty()){
    auto found=filesIn(managed/".batbox"/"agents","policySettings",inputs);
    rows.insert(rows.end(),found.begin(),found.end());
  }
  if(!inputs.config.legacyConfigDirectory.empty()){
    auto legacy=filesIn(inputs.config.legacyConfigDirectory/"agents",
      "userSettings",inputs);
    rows.insert(rows.end(),legacy.begin(),legacy.end());
  }
  auto user=filesIn(inputs.config.configPath.parent_path()/"agents",
    "userSettings",inputs);
  rows.insert(rows.end(),user.begin(),user.end());
  auto canonicalUser=filesIn(application::configurationDirectory(inputs.config)/
    "agents","userSettings",inputs);
  rows.insert(rows.end(),canonicalUser.begin(),canonicalUser.end());
  for(const auto& dir:projectDirs(inputs)){
    auto found=filesIn(dir,"projectSettings",inputs);
    rows.insert(rows.end(),found.begin(),found.end());
  }
  std::set<Path> seen;
  std::vector<File> unique;
  for(auto& row:rows){
    std::error_code error;
    const auto canonical=std::filesystem::canonical(row.path,error);
    if(!error&&!seen.insert(canonical).second)continue;
    unique.push_back(std::move(row));
  }
  return unique;
}
void upsert(Json& active,std::map<std::string,std::size_t>& index,
    const Json& definition){
  const auto& name=value::property(definition,"agentType");
  if(!name.is_string())return;
  const auto key=name.get<std::string>();
  if(const auto found=index.find(key);found!=index.end())active[found->second]=definition;
  else {index.emplace(key,active.size());active.push_back(definition);}
}
}
Json loadBatDefinitions(const DefinitionInputs& inputs){
  Json all=builtInDefinitions(inputs.config.builtinExplorePlanBats);
  Json failed=Json::array();
  for(const auto& file:loadFiles(inputs)){
    try{
      const auto parsed=parseMarkdownBat(file.path,file.base,file.source,
        inputs.diagnostic);
      if(!parsed.is_null())all.push_back(parsed);
    }catch(const std::exception& error){
      const auto path=runtime::pathToUtf8(file.path);
      failed.push_back(Json{{"path",path},{"error",error.what()}});
      note(inputs,"Failed to parse agent "+path+": "+error.what());
    }
  }
  if(inputs.config.customBats.is_object())
    for(auto it=inputs.config.customBats.begin();
        it!=inputs.config.customBats.end();++it){
      const auto parsed=parseJsonBat(it.key(),it.value(),inputs.diagnostic);
      if(!parsed.is_null())all.push_back(parsed);
    }
  Json active=Json::array();
  std::map<std::string,std::size_t> index;
  for(const std::string source:{"built-in","plugin","userSettings",
      "projectSettings","flagSettings","policySettings"})
    for(const auto& definition:all)
      if(value::property(definition,"source")==source)
        upsert(active,index,definition);
  Json result={{"activeAgents",std::move(active)},
    {"allAgents",std::move(all)}};
  if(!failed.empty())result["failedFiles"]=std::move(failed);
  return result;
}
}
