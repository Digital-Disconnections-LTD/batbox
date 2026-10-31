#include "runtime/AsyncLaunch.hpp"
#include "diagnostics/DebugLog.hpp"
#include "cleanup/Roster.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace batbox::diagnostics {
namespace {
void append(const std::string& path,const std::string& content){
  std::ofstream out(path,std::ios::binary|std::ios::app);
  if(!out)throw std::runtime_error("Cannot open debug log");
  out.write(content.data(),static_cast<std::streamsize>(content.size()));
  if(!out)throw std::runtime_error("Cannot append debug log");
}
template<class F>DebugMission asyncFilesystem(event_loop::Loop& loop,F operation){
  DebugMission mission(loop);
  ::batbox::runtime::contextThread([mission,operation=std::move(operation)]()mutable{
    try{operation();mission.resolve({});}
    catch(...){try{mission.reject(std::current_exception());}catch(...){}}
  }).detach();
  return mission;
}
}
DebugLogServices indigenousDebugLogServices(event_loop::Loop& loop,
    std::function<std::string()> timestampISO,
    std::function<std::string()> flightId,
    std::function<void(std::function<DebugMission()>)> registerCleanup,
    std::vector<std::string> argv){
  DebugLogServices out;
  out.loop=&loop;
  out.env=[](const std::string& key)->std::optional<std::string>{
    const auto* value=std::getenv(key.c_str());
    return value?std::optional<std::string>(value):std::nullopt;
  };
  out.argv=[argv=std::move(argv)]{return argv;};
  out.isInternalUser=[](){
    const auto* value=std::getenv("BATBOX_INTERNAL_USER");
    return value&&*value&&std::string(value)!="0";
  };
  out.nodeVersionAvailable=[](){return true;};
  out.timestampISO=std::move(timestampISO);
  out.configHomeDir=[](){
    if(const auto* configured=std::getenv("BATBOX_CONFIG_DIR"))return std::string(configured);
    const auto* home=std::getenv("HOME");
    return (std::filesystem::path(home?home:".")/".batbox").string();
  };
  out.flightId=std::move(flightId);
  out.writeToStderr=[](const std::string& content){
    if(std::fwrite(content.data(),1,content.size(),stderr)!=content.size())
      throw std::runtime_error("Cannot write debug output to stderr");
  };
  out.mkdirSync=[](const std::string& path){
    if(!std::filesystem::create_directory(path))
      throw std::runtime_error("Debug directory already exists");
  };
  out.appendFileSync=append;
  out.mkdirAsync=[&loop](const std::string& path){
    return asyncFilesystem(loop,[path]{std::filesystem::create_directories(path);});
  };
  out.appendFileAsync=[&loop](const std::string& path,const std::string& content){
    return asyncFilesystem(loop,[path,content]{append(path,content);});
  };
  out.unlinkAsync=[&loop](const std::string& path){
    return asyncFilesystem(loop,[path]{
      if(!std::filesystem::remove(path))throw std::runtime_error("No debug symlink");
    });
  };
  out.symlinkAsync=[&loop](const std::string& target,const std::string& link){
    return asyncFilesystem(loop,[target,link]{std::filesystem::create_symlink(target,link);});
  };
  out.registerCleanup=std::move(registerCleanup);
  return out;
}
DebugLogServices indigenousDebugLogServices(event_loop::Loop& loop,
    cleanup::Roster& roster,
    std::function<std::string()> timestampISO,
    std::function<std::string()> flightId,
    std::vector<std::string> argv){
  return indigenousDebugLogServices(loop,std::move(timestampISO),
    std::move(flightId),[&roster](std::function<DebugMission()> callback){
      roster.registerCleanup(std::move(callback));
    },std::move(argv));
}
}
