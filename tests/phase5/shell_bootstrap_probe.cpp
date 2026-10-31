#include "shell/ShellBootstrap.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace batbox;
namespace {
bool contains(const Json& list,const std::string& value){
  for(const auto& item:list)if(item==value)return true;return false;
}
Json run(const Json& item){
  event_loop::Loop loop;Json logs=Json::array();
  shell::DiscoveryServices services;
  if(item.contains("override")&&!item["override"].is_null())
    services.shellOverride=item["override"].get<std::string>();
  if(item.contains("preferred")&&!item["preferred"].is_null())
    services.preferredShell=item["preferred"].get<std::string>();
  services.isExecutable=[&](const std::string& path){
    logs.push_back("access:"+path);
    if(contains(item.value("access",Json::array()),path))return true;
    logs.push_back("exec:"+path+":--version");
    return contains(item.value("fallback",Json::array()),path);
  };
  services.which=[&](const std::string& name){
    logs.push_back("which:"+name);
    event_loop::Deferred<std::optional<std::string>> result(loop);
    if(item.contains("whichError")){
      result.reject(std::make_exception_ptr(std::runtime_error(item["whichError"].get<std::string>())));
    }else{
      const auto key=name=="zsh"?"whichZsh":"whichBash";
      result.resolve(item.contains(key)&&!item[key].is_null()?
        std::optional(item[key].get<std::string>()):std::nullopt);
    }
    return result;
  };
  services.debug=[&](const std::string& value){logs.push_back("debug:"+value);};
  services.error=[&](const std::string& value){logs.push_back("error:"+value);};
  auto pending=shell::findSuitableShellAsync(loop,services);
  std::string value;
  pending.then([&](const std::string* result,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){value="ERROR:"+std::string(e.what());}}
    else value=*result;
  });
  loop.run();
  return Json{{"name",item.at("name")},{"value",value},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);
  const std::string input(std::istreambuf_iterator<char>{stream},{});
  const auto cases=value::parse(input);
  Json output=Json::array();for(const auto& item:cases)output.push_back(run(item));
  std::cout<<value::stringify(output)<<'\n';
}
