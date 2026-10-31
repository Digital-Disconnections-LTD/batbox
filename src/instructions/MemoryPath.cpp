#include "instructions/MemoryPath.hpp"
#include "runtime/PathSanitization.hpp"
#include "runtime/IndigenousTime.hpp"
#include "runtime/Unicode.hpp"
#include "shell/PosixPath.hpp"
#include "utils/envUtils.hpp"
#include <unicode/unorm2.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace batbox::instructions {
namespace {
const Json& env(const MemoryPathServices& services,const std::string& key){
  return value::property(services.environment,key);
}
std::optional<std::string> stringValue(const Json& value){
  return value.is_string()?std::optional<std::string>(value.get<std::string>()):std::nullopt;
}
std::string normalizePath(const std::string& value){return shell::joinPosixPath(value,"");}
std::string join(const std::string& first,const std::string& second){
  return shell::joinPosixPath(first,second);
}
std::string nfc(const std::string& value){return value::normalizeNFC(value); }

}
bool isAutoMemoryEnabled(const MemoryPathServices& services){
  const auto& disable=env(services,services.keys.disableAutoMemory);
  if(isEnvTruthy(disable))return false;
  if(isEnvDefinedFalsy(disable))return true;
  if(isEnvTruthy(env(services,services.keys.simpleMode)))return false;
  if(isEnvTruthy(env(services,services.keys.remoteMode))&&
      !value::truthy(env(services,services.keys.remoteMemoryDir)))return false;
  const auto& enabled=value::property(services.initialSettings,"autoMemoryEnabled");
  if(!enabled.is_null())return value::truthy(enabled);
  return true;
}
std::string getMemoryBaseDir(const MemoryPathServices& services){
  const auto& remote=env(services,services.keys.remoteMemoryDir);
  if(value::truthy(remote)&&remote.is_string())return remote.get<std::string>();
  return services.configHomeDir;
}
std::optional<std::string> validateMemoryPath(const std::optional<std::string>& raw,
  bool expandTilde,const std::string& homeDirectory){
  if(!raw||raw->empty())return std::nullopt;
  auto candidate=*raw;
  if(expandTilde&&(candidate.starts_with("~/")||candidate.starts_with("~\\"))){
    const auto rest=candidate.substr(2);
    const auto restNorm=normalizePath(rest.empty()?".":rest);
    if(restNorm=="."||restNorm=="..")return std::nullopt;
    candidate=join(homeDirectory,rest);
  }
  auto normalized=normalizePath(candidate);
  while(!normalized.empty()&&(normalized.back()=='/'||normalized.back()=='\\'))
    normalized.pop_back();
  if(normalized.empty()||normalized[0]!='/'||value::stringLength(normalized)<3||
      normalized.starts_with("\\\\")||normalized.starts_with("//")||
      normalized.find('\0')!=std::string::npos)return std::nullopt;
  return nfc(normalized+"/");
}
std::optional<std::string> getAutoMemPathOverride(const MemoryPathServices& services){
  return validateMemoryPath(stringValue(env(services,services.keys.fullPathOverride)),
    false,services.homeDirectory);
}
std::optional<std::string> getAutoMemPathSetting(const MemoryPathServices& services){
  for(const char* source:{"policySettings","flagSettings","localSettings","userSettings"}){
    const auto& value=value::property(value::property(services.settingsBySource,source),
      "autoMemoryDirectory");
    if(!value.is_null())return validateMemoryPath(stringValue(value),true,
      services.homeDirectory);
  }
  return std::nullopt;
}
bool hasAutoMemPathOverride(const MemoryPathServices& services){
  return getAutoMemPathOverride(services).has_value();
}
std::string sanitizeMemoryProjectPath(const std::string& name,
  const std::function<std::string(const std::string&)>& longPathHash){
  return runtime::sanitizePath(name,longPathHash);
}
std::string getAutoMemPath(const MemoryPathServices& services,MemoryPathStash& stash){
  if(const auto at=stash.byProjectRoot.find(services.projectRoot);
      at!=stash.byProjectRoot.end())return at->second;
  auto selected=getAutoMemPathOverride(services);
  if(!selected)selected=getAutoMemPathSetting(services);
  if(!selected){
    if(!services.findCanonicalGitRoot)
      throw std::invalid_argument("Missing canonical Git root import");
    const auto gitRoot=services.findCanonicalGitRoot(services.projectRoot);
    const auto project=gitRoot.value_or(services.projectRoot);
    selected=nfc(join(join(join(getMemoryBaseDir(services),"projects"),
      sanitizeMemoryProjectPath(project,services.longPathHash)),"memory")+"/");
  }
  stash.byProjectRoot[services.projectRoot]=*selected;
  return *selected;
}
std::string getAutoMemEntrypoint(const MemoryPathServices& services,MemoryPathStash& stash){
  return join(getAutoMemPath(services,stash),"MEMORY.md");
}
std::string getAutoMemDailyLogPath(const MemoryPathServices& services,MemoryPathStash& stash,
  std::chrono::system_clock::time_point date){
  const std::time_t time=std::chrono::system_clock::to_time_t(date);
  std::tm local{};
  if(!runtime::localtimeIndigenous(time,local))
    throw std::runtime_error("Invalid local date");
  std::ostringstream year,month,day;
  year<<std::setw(4)<<std::setfill('0')<<local.tm_year+1900;
  month<<std::setw(2)<<std::setfill('0')<<local.tm_mon+1;
  day<<std::setw(2)<<std::setfill('0')<<local.tm_mday;
  return join(join(join(join(getAutoMemPath(services,stash),"logs"),year.str()),
    month.str()),year.str()+"-"+month.str()+"-"+day.str()+".md");
}
bool isAutoMemPath(const std::string& absolutePath,const MemoryPathServices& services,
  MemoryPathStash& stash){
  return normalizePath(absolutePath).starts_with(getAutoMemPath(services,stash));
}
} // namespace batbox::instructions
