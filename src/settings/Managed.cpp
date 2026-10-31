#include "settings/Managed.hpp"
#include "settings/Merge.hpp"
#include <algorithm>
#include <filesystem>

namespace batbox::settings {
namespace {
bool hasSettings(const Json& value){
  return value.is_object()&&!value.empty();
}
bool matching(const std::filesystem::directory_entry& entry){
  const auto name=entry.path().filename().string();
  return (entry.is_regular_file()||entry.is_symlink())&&
    name.size()>=5&&name.ends_with(".json")&&name[0]!='.';
}
}
ParsedSettings loadManagedFileSettings(Paths& paths,Stash& stash,
  const ParseServices& services){
  Json errors=Json::array(),merged=Json::object();
  bool found=false;
  const auto base=parseSettingsFile(paths.getManagedSettingsFilePath(),
    stash,services);
  for(const auto& error:base.errors)errors.push_back(error);
  if(hasSettings(base.settings)){
    merged=mergeSettings(merged,base.settings);found=true;
  }
  const auto dropInDir=paths.getManagedSettingsDropInDir();
  try{
    std::vector<std::string> names;
    for(const auto& entry:std::filesystem::directory_iterator(dropInDir))
      if(matching(entry))names.push_back(entry.path().filename().string());
    std::sort(names.begin(),names.end());
    for(const auto& name:names){
      const auto result=parseSettingsFile((std::filesystem::path(dropInDir)/
        name).string(),stash,services);
      for(const auto& error:result.errors)errors.push_back(error);
      if(hasSettings(result.settings)){
        merged=mergeSettings(merged,result.settings);found=true;
      }
    }
  }catch(const std::filesystem::filesystem_error& error){
    if(error.code()!=std::errc::no_such_file_or_directory&&
       error.code()!=std::errc::not_a_directory&&services.logFileError)
      services.logFileError(dropInDir,error.what());
  }
  return {found?merged:Json(nullptr),errors};
}
ManagedPresence getManagedFileSettingsPresence(Paths& paths,Stash& stash,
  const ParseServices& services){
  const auto base=parseSettingsFile(paths.getManagedSettingsFilePath(),
    stash,services);
  ManagedPresence presence{hasSettings(base.settings),false};
  try{
    for(const auto& entry:std::filesystem::directory_iterator(
      paths.getManagedSettingsDropInDir()))if(matching(entry)){
      presence.hasDropIns=true;break;
    }
  }catch(...){/* Ignore directory enumeration errors here. */}
  return presence;
}
}
