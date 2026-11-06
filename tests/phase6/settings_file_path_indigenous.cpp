#include "application_gadgets/DefaultInputs.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const std::string& message){
  if(!value)throw std::runtime_error(message);
}
}
int main(){
  using batbox::application_gadgets::isSettingsFile;
  for(const auto* path:{"/home/user/.batbox/settings.json",
      "/work/app/.batbox/settings.local.json",
      "/work/app/.BATBOX/Settings.JSON",
      "/work/app/sub/../.batbox/settings.json"})
    require(isSettingsFile(path),std::string("settings file not recognised: ")+path);
  for(const auto* path:{"/home/user/.claude/settings.json",
      "/work/app/.claude/settings.local.json",
      "/work/app/settings.json",
      "/work/app/.batbox/settings.json.bak",
      "/work/app/.batbox/native-client.json"})
    require(!isSettingsFile(path),std::string("not a Batbox settings file: ")+path);
  std::cout<<"Batbox settings file paths passed\n";
}
