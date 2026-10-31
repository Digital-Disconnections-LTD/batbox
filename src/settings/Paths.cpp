#include "settings/Paths.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::settings {
namespace {
std::string join(const std::string& left,const std::string& right){
  return (std::filesystem::path(left)/right).string();
}
std::string resolve(const std::string& path,const std::string& cwd){
  const std::filesystem::path p(path);
  return (p.is_absolute()?p:std::filesystem::path(cwd)/p)
    .lexically_normal().string();
}
}
Paths::Paths(PathServices services):services_(std::move(services)){}
std::string Paths::getManagedFilePath(){
  if(managedPath_)return *managedPath_;
  if(services_.internalUser&&services_.managedPathOverride&&
     !services_.managedPathOverride->empty())
    managedPath_=*services_.managedPathOverride;
  else if(services_.platform=="macos")
    managedPath_="/Library/Application Support/Batbox";
  else if(services_.platform=="windows")
    managedPath_="C:\\Program Files\\Batbox";
  else managedPath_="/etc/batbox";
  return *managedPath_;
}
std::string Paths::getManagedSettingsDropInDir(){
  if(!dropInDir_)dropInDir_=join(getManagedFilePath(),"managed-settings.d");
  return *dropInDir_;
}
std::string Paths::getManagedSettingsFilePath(){
  return join(getManagedFilePath(),"managed-settings.json");
}
std::string Paths::getSettingsRootPathForSource(Source source) const{
  switch(source){
    case Source::User:return resolve(services_.configHome,services_.processCwd);
    case Source::Policy:
    case Source::Project:
    case Source::Local:return resolve(services_.originalCwd,services_.processCwd);
    case Source::Flag:
      return services_.flagSettingsPath&&
        !services_.flagSettingsPath->empty()
        ?std::filesystem::path(resolve(*services_.flagSettingsPath,
          services_.processCwd)).parent_path().string()
        :resolve(services_.originalCwd,services_.processCwd);
  }
  throw std::invalid_argument("Unknown settings source");
}
std::string Paths::getUserSettingsFilePath() const{
  return services_.coworkPlugins||services_.coworkEnvironment
    ?"cowork_settings.json":"settings.json";
}
std::optional<std::string> Paths::getSettingsFilePathForSource(Source source){
  switch(source){
    case Source::User:return join(getSettingsRootPathForSource(source),
      getUserSettingsFilePath());
    case Source::Project:
    case Source::Local:return join(getSettingsRootPathForSource(source),
      getRelativeSettingsFilePathForSource(source));
    case Source::Policy:return getManagedSettingsFilePath();
    case Source::Flag:return services_.flagSettingsPath;
  }
  throw std::invalid_argument("Unknown settings source");
}
std::string Paths::getRelativeSettingsFilePathForSource(Source source) const{
  switch(source){
    case Source::Project:return ".batbox/settings.json";
    case Source::Local:return ".batbox/settings.local.json";
    default:throw std::invalid_argument("Expected project or local settings source");
  }
}
}
