#include "diagnostics/DebugLog.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>

int main(){
  using namespace batbox;
  using namespace batbox::diagnostics;
  const auto input=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  std::map<std::string,std::string> env;
  for(auto it=input["env"].begin();it!=input["env"].end();++it)
    env[it.key()]=it.value().get<std::string>();
  std::vector<std::string> args;
  for(const auto& arg:input["argv"])args.push_back(arg.get<std::string>());
  Json writes=Json::array(),stderrs=Json::array(),output=Json::array();
  event_loop::Loop loop;
  DebugLogServices services;
  services.loop=&loop;
  services.env=[&](const std::string& name)->std::optional<std::string>{
    const auto it=env.find(name);
    return it==env.end()?std::nullopt:std::optional(it->second);
  };
  services.argv=[&]{return args;};
  services.isInternalUser=[&]{return env.contains("USER_TYPE")&&env["USER_TYPE"]=="ant";};
  services.nodeVersionAvailable=[]{return true;};
  services.timestampISO=[]{return "2026-03-31T12:34:56.000Z";};
  services.configHomeDir=[]{return "/batbox";};
  services.flightId=[]{return "session-1";};
  services.writeToStderr=[&](const std::string& line){stderrs.push_back(line);};
  services.mkdirSync=[](const std::string&){};
  services.appendFileSync=[&](const std::string& path,const std::string& line){
    writes.push_back(Json::array({path,line}));};
  const auto ready=[&](){DebugMission mission(loop);mission.resolve({});return mission;};
  services.mkdirAsync=[&](const std::string&){return ready();};
  services.appendFileAsync=[&](const std::string& path,const std::string& line){
    writes.push_back(Json::array({path,line}));return ready();};
  services.unlinkAsync=[&](const std::string&){return ready();};
  services.symlinkAsync=[&](const std::string&,const std::string&){return ready();};
  services.registerCleanup=[](std::function<DebugMission()>){};
  DebugLog logger(std::move(services));
  for(const auto& action:input["actions"]){
    if(action.contains("argv")){
      args.clear();
      for(const auto& arg:action["argv"])args.push_back(arg.get<std::string>());
    }
    if(action.contains("env"))for(auto it=action["env"].begin();it!=action["env"].end();++it){
      if(it.value().is_null())env.erase(it.key());
      else env[it.key()]=it.value().get<std::string>();
    }
    Json value=nullptr;
    const auto kind=action["kind"].get<std::string>();
    if(kind=="min")value=logger.getMinDebugLogLevel();
    if(kind=="mode")value=logger.isDebugMode();
    if(kind=="enable")value=logger.enableDebugLogging();
    if(kind=="filter"){
      const auto f=logger.getDebugFilter();
      if(f)value={{"include",f->include},{"exclude",f->exclude},{"isExclusive",f->isExclusive}};
    }
    if(kind=="stderr")value=logger.isDebugToStdErr();
    if(kind=="file"){
      if(const auto f=logger.getDebugFilePath())value=*f;
    }
    if(kind=="should")value=logger.shouldLogDebugMessage(action["message"].get<std::string>());
    if(kind=="setFormat")logger.setHasFormattedOutput(action["value"].get<bool>());
    if(kind=="format")value=logger.getHasFormattedOutput();
    if(kind=="log")logger.logForDebugging(action["message"].get<std::string>(),
      action.value("level",std::string("debug")));
    if(kind=="path")value=logger.getDebugLogPath();
    if(kind=="flush"){
      auto pending=logger.flushDebugLogs();
      for(int i=0;i<16&&!pending.settled();++i)loop.runReady();
      loop.runReady();
    }
    if(kind=="immediate")loop.runReady();
    output.push_back({{"value",value},{"writes",writes},{"stderrs",stderrs}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
