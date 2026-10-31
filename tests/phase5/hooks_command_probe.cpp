#include "instructions/hooks/runtime/IndigenousCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

extern char** environ;
using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    event_loop::Loop loop;
    hook::IndigenousCommandConfig config;
    config.getProjectRoot=[](){return std::string("/tmp");};
    config.getCwd=[cwd=item.at("cwd").get<std::string>()](){return cwd;};
    config.getOriginalCwd=[cwd=item.value("originalCwd",item.at("cwd").get<std::string>())](){return cwd;};
    config.subprocessEnv=[](){
      std::map<std::string,std::string> env;
      for(char** entry=environ;*entry;++entry){
        std::string text=*entry;const auto pos=text.find('=');
        if(pos!=std::string::npos)env[text.substr(0,pos)]=text.substr(pos+1);
      }
      return env;
    };
    config.pathExists=[&loop](const std::string& path){
      event_loop::Deferred<bool> result(loop);
      result.resolve(std::filesystem::exists(path));return result;
    };
    config.shellPrefix=[](){return std::optional<std::string>{};};
    config.logDebug=[](const std::string&,const std::string&){};
    auto execute=hook::indigenousCommandHook(loop,config);
    hook::CommandRequest request;
    request.hook={{"type","command"},{"command",item.at("command")}};
    request.hookEvent="InstructionsLoaded";
    request.hookName="InstructionsLoaded:session_start";
    request.jsonInput=item.at("input").get<std::string>();
    request.uuid="hook-id";
    Json row={{"name",item.at("name")}};
    execute(std::move(request)).then([&row](const hook::CommandOutcome* value,
      const std::exception_ptr& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& problem){
        row["error"]=problem.what();}return;}
      row["stdout"]=value->stdoutText;row["stderr"]=value->stderrText;
      row["output"]=value->output;row["status"]=value->status;
      row["aborted"]=value->aborted;
    });
    loop.run();rows.push_back(std::move(row));
  }
  std::cout<<value::stringify(rows)<<'\n';
}
