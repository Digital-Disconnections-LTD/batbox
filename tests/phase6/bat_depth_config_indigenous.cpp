#include "application/Config.hpp"
#include "runtime/IndigenousPath.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace batbox;
namespace {
void check(bool value,const std::string& message){
  if(!value)throw std::logic_error(message);
}
void setVariable(const char* name,const std::string& value){
#ifdef _WIN32
  _putenv_s(name,value.c_str());
#else
  if(value.empty())unsetenv(name);else setenv(name,value.c_str(),1);
#endif
}
void write(const std::filesystem::path& path,const Json& document){
  std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<document.dump();
}
std::size_t depth(const std::vector<std::string>& arguments){
  return application::loadConfig(arguments).maxBatDepth;
}
std::string failure(const std::vector<std::string>& arguments){
  try{(void)application::loadConfig(arguments);}
  catch(const std::invalid_argument& error){return error.what();}
  return {};
}
}
int main(){
  const auto root=std::filesystem::temp_directory_path()/("batbox-bat-depth-"+
    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  try{
    setVariable("BATBOX_CONFIG_DIR",runtime::pathToUtf8(root));
    for(const auto* name:{"BATBOX_PROFILE","BATBOX_MODEL","BATBOX_BASE_URL",
        "BATBOX_AUTH_TOKEN","BATBOX_API_KEY_HELPER","BATBOX_MAX_BAT_DEPTH"})
      setVariable(name,"");
    check(depth({})==3,"Bats nest three levels deep by default");
    write(root/"settings.json",Json{{"max_bat_depth",2}});
    check(depth({})==2,"settings.json sets max_bat_depth");
    write(root/"native-client.json",Json{{"max_bat_depth",4}});
    check(depth({})==4,"native-client.json overrides settings.json like the other limits");
    setVariable("BATBOX_MAX_BAT_DEPTH","1");
    check(depth({})==1,"BATBOX_MAX_BAT_DEPTH overrides the files");
    check(depth({"--max-bat-depth","5"})==5,"--max-bat-depth overrides the environment");
    check(depth({"--max-bat-depth","0"})==0,"0 is accepted and means Bats cannot launch Bats");
    setVariable("BATBOX_MAX_BAT_DEPTH","");
    for(const auto* value:{"-1","two","2.5",""}){
      const auto error=failure({"--max-bat-depth",value});
      check(error.find("--max-bat-depth")!=std::string::npos,
        std::string("--max-bat-depth ")+value+" must be refused, got: "+error);
    }
    check(failure({"--max-bat-depth"}).find("requires a value")!=std::string::npos,
      "A missing value must be reported");
    write(root/"native-client.json",Json{{"max_bat_depth",-2}});
    check(failure({}).find("max_bat_depth")!=std::string::npos,
      "A negative max_bat_depth in the configuration must be refused");
    std::filesystem::remove(root/"native-client.json");
    setVariable("BATBOX_MAX_BAT_DEPTH","deep");
    check(failure({}).find("BATBOX_MAX_BAT_DEPTH")!=std::string::npos,
      "An unreadable BATBOX_MAX_BAT_DEPTH must be refused");
    setVariable("BATBOX_MAX_BAT_DEPTH","");
    const auto help=application::commandLineHelp();
    check(help.find("--max-bat-depth N")!=std::string::npos&&
      help.find("--max-concurrent-subagents N")<help.find("--max-bat-depth N"),
      "--help lists --max-bat-depth after the other Bat limits");
    std::error_code ignored;std::filesystem::remove_all(root,ignored);
    std::cout<<"max_bat_depth defaults to 3 and layers settings.json, native-client.json, "
      "BATBOX_MAX_BAT_DEPTH and --max-bat-depth\n";
    return 0;
  }catch(const std::exception& error){
    std::error_code ignored;std::filesystem::remove_all(root,ignored);
    std::cerr<<error.what()<<"\n";return 1;
  }
}
