#include "instructions/hooks/runtime/IndigenousCommandPreparation.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array();
  hook::IndigenousCommandConfig config;
  config.shellPath="/bin/sh";
  config.getProjectRoot=[&]{calls.push_back("project-root");return "/project";};
  config.getCwd=[&]{calls.push_back("cwd");return "/workspace";};
  config.getOriginalCwd=[&]{calls.push_back("original-cwd");return "/original";};
  config.subprocessEnv=[&]{calls.push_back("env");
    return std::map<std::string,std::string>{{"PATH","/usr/bin:/bin"}};};
  config.pathExists=[&](const std::string& path){
    calls.push_back("exists:"+path);
    event_loop::Deferred<bool> result(loop);
    result.resolve(path==item.value("pluginRoot",std::string{})?
      item.value("pluginExists",true):item.value("cwdExists",true));
    return result;};
  config.shellPrefix=[](){return std::optional<std::string>{};};
  config.getPluginDataDir=[&](const std::string& id){
    calls.push_back("data-dir:"+id);return "/plugins/data";};
  config.loadPluginOptions=[&](const std::string& id){
    calls.push_back("options:"+id);return Json{{"COLOR","blue"}};};
  config.substituteUserConfig=[&](const std::string& command,const Json& options){
    calls.push_back("substitute");
    auto result=command;const std::string token="${user_config.COLOR}";
    const auto offset=result.find(token);
    if(offset!=std::string::npos)result.replace(offset,token.size(),
      options.at("COLOR").get<std::string>());
    return result;};
  config.getHookEnvFilePath=[&](const std::string& event,std::size_t index){
    calls.push_back("env-file:"+event+":"+std::to_string(index));
    event_loop::Deferred<std::string> result(loop);
    result.resolve("/tmp/hook-env");return result;};
  config.logDebug=[&](const std::string& message,const std::string& level){
    calls.push_back("log:"+level+":"+message);};
  hook::FullCommandRequest request;
  request.base.hook={{"command",item.at("command")}};
  auto command=request.base.hook.at("command").get<std::string>();
  for(const auto& [from,to]:{
      std::pair<std::string,std::string>{"${ROOT}","${BATBOX_PLUGIN_ROOT}"},
      {"${DATA}","${BATBOX_PLUGIN_DATA}"}}){
    const auto offset=command.find(from);
    if(offset!=std::string::npos)command.replace(offset,from.size(),to);
  }
  request.base.hook["command"]=command;
  if(item.contains("timeout"))request.base.hook["timeout"]=item.at("timeout");
  request.base.hookEvent=item.at("hookEvent").get<std::string>();
  if(item.contains("hookIndex"))request.hookIndex=item.at("hookIndex").get<std::size_t>();
  if(item.contains("pluginRoot"))request.base.pluginRoot=item.at("pluginRoot").get<std::string>();
  if(item.contains("pluginId"))request.base.pluginId=item.at("pluginId").get<std::string>();
  if(item.contains("skillRoot"))request.skillRoot=item.at("skillRoot").get<std::string>();
  Json result=Json::object();
  hook::prepareIndigenousHookCommand(loop,config,request).then(
    [&](const hook::IndigenousProcessRequest* value,const std::exception_ptr& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
        result["error"]=e.what();}return;}
      Json env=Json::object();
      for(const auto& [key,text]:value->environment)env[key]=text;
      result["result"]={{"finalCommand",value->command},{"envVars",env},
        {"safeCwd",value->cwd},{"hookTimeoutMs",value->timeoutMs}};
    });
  loop.run();
  result["name"]=item.at("name");result["calls"]=calls;
  return result;
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
