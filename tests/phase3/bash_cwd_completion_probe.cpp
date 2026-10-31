#include "shell/ShellCwdCompletion.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string data(std::istreambuf_iterator<char>{file},{});
  const auto cases=batbox::value::parse(data);batbox::Json output=batbox::Json::array();
  for(const auto& item:cases){
    std::vector<std::string> events;
    batbox::shell::CwdCompletionServices deps;
    deps.windows=item.value("windows",false);
    deps.posixPathToIndigenous=[&](const std::string& value){events.push_back("convert:"+value);
      return "native:"+value;};
    deps.cleanupAfterSandboxCommand=[&]{events.push_back("cleanup");
      if(item.value("cleanupThrows",false))throw std::runtime_error("cleanup failed");};
    deps.readFileUtf8=[&](const std::string&){events.push_back("read");
      if(item.value("readThrows",false))throw std::runtime_error("missing");
      return item.value("newCwd",std::string{});};
    deps.normalizeNFC=[](const std::string& value){return value;};
    deps.setCwd=[&](const std::string& next,const std::string& previous){
      events.push_back("set:"+previous+">"+next);};
    deps.invalidateFlightEnvStash=[&]{events.push_back("invalidate");};
    deps.onCwdChangedForHooks=[&](const std::string& previous,const std::string& next){
      events.push_back("hook:"+previous+">"+next);};
    deps.unlinkFile=[&](const std::string&){events.push_back("unlink");
      if(item.value("unlinkThrows",false))throw std::runtime_error("missing");};
    batbox::shell::ExecResult result;if(item.value("background",false))result.backgroundMissionId="bg1";
    try{batbox::shell::completeShellCwd(item.value("result",true)?&result:nullptr,
      item.value("prevent",false),item.value("sandbox",false),"/tmp/cwd",
      item.value("oldCwd",std::string("/project")),deps);
      output.push_back({{"events",events}});
    }catch(const std::exception& error){output.push_back({{"events",events},{"error",error.what()}});}
  }
  std::cout<<batbox::value::stringify(output)<<'\n';
}
