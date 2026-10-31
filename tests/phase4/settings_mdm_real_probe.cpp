#include "settings/mdm/Parse.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
void require(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
void write(const std::filesystem::path& path,const std::string& text){
  std::ofstream output(path,std::ios::binary|std::ios::trunc);
  if(!output)throw std::runtime_error("open failed");
  output<<text;
  if(!output)throw std::runtime_error("write failed");
}
}
int main(){
  using namespace batbox;
  using namespace batbox::settings;
  using namespace batbox::settings::mdm;
  const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-mdm-"+std::to_string(::getpid())+"-"+std::to_string(stamp));
  std::filesystem::create_directories(root);
  try{
    PathServices pathOptions;
    pathOptions.internalUser=true;
    pathOptions.managedPathOverride=root.string();
    Paths paths(pathOptions);
    persistence::SafeJsonParser safe([](const std::string&){});
    settings::ParseServices schema;
    schema.filterInvalidPermissionRules=[](Json&,const std::string&){
      return Json::array();};
    schema.validateSettingsSchema=[](const Json& value){
      return SchemaResult{true,value,Json::array()};};
    schema.formatZodError=[](const Json&,const std::string&){
      return Json::array();};
    auto parser=indigenousParseServices(paths,safe,schema);
    const auto hkcu=std::string("  Settings REG_SZ {\"fromHKCU\":true}");
    RawReadResult raw;raw.hkcuStdout=hkcu;
    require(!hasManagedSettingsFile(parser),"missing managed file");
    require(consumeRawReadResult(raw,parser).hkcu.settings.contains("fromHKCU"),
      "HKCU should apply before managed file exists");
    write(root/"managed-settings.json","{}");
    require(!hasManagedSettingsFile(parser),"empty managed file");
    write(root/"managed-settings.json","{\"admin\":true}");
    require(hasManagedSettingsFile(parser),"managed file not discovered");
    require(consumeRawReadResult(raw,parser).hkcu.settings.empty(),
      "managed file must suppress HKCU");
    std::filesystem::remove(root/"managed-settings.json");
    std::filesystem::create_directories(root/"managed-settings.d");
    write(root/"managed-settings.d"/"a.json","{\"policy\":1}");
    require(hasManagedSettingsFile(parser),"drop-in not discovered");
    std::filesystem::remove(root/"managed-settings.d"/"a.json");
    write(root/"source.json","{\"policy\":2}");
    std::filesystem::create_symlink(root/"source.json",
      root/"managed-settings.d"/"alias.json");
    require(hasManagedSettingsFile(parser),"drop-in symlink not discovered");
    std::filesystem::remove(root/"managed-settings.d"/"alias.json");
    write(root/"managed-settings.d"/".hidden.json","{\"ignore\":1}");
    write(root/"managed-settings.d"/"upper.JSON","{\"ignore\":1}");
    require(!hasManagedSettingsFile(parser),"ignored drop-ins applied");
    std::filesystem::remove_all(root);
    std::cout<<"Native MDM managed-file fixture passed\n";
  }catch(...){std::filesystem::remove_all(root);throw;}
}
