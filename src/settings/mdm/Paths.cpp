#include "settings/mdm/RawRead.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::settings::mdm {
std::vector<PlistPath> getMacOSPlistPaths(const RawReadServices& services){
  std::string username;
  try{if(!services.username)
      throw std::invalid_argument("Missing MDM username binding");
    username=services.username();
  }catch(...){username.clear();}
  std::vector<PlistPath> result;
  const auto base=std::string("/Library/Managed Preferences/");
  const auto name=services.names.macPreferenceDomain+".plist";
  if(!username.empty())result.push_back({base+username+"/"+name,
    "per-user managed preferences"});
  result.push_back({base+name,"device-level managed preferences"});
  if(services.internalUser){
    if(!services.homedir)
      throw std::invalid_argument("Missing MDM home directory binding");
    result.push_back({(std::filesystem::path(services.homedir())/"Library"/
      "Preferences"/name).string(),"user preferences (internal-only)"});
  }
  return result;
}
}
