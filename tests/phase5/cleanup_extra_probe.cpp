#include "cleanup/ExtraRetention.hpp"
#include "runtime/JsonValue.hpp"
#include "shell/PosixPath.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <system_error>

using namespace batbox;
namespace {
bool contains(const Json& list,const std::string& value){
  for(const auto& entry:list)if(entry==value)return true;return false;
}
Json evaluate(const Json& item){
  event_loop::Loop loop;Json logs=Json::array();
  const auto dirs=item.value("dirs",Json::object());
  const auto mtimes=item.value("mtime",Json::object());
  cleanup::ExtraServices services;
  services.getCutoffDateMs=[&](){
    if(item.value("cutoffError",false))throw std::runtime_error("cutoff failed");
    return cleanup::cutoffDateMs(
    *cleanup::indigenousLogDateMs("2026-10-28T12:00:00.000Z"),30);};
  services.getProjectsDir=[&]{logs.push_back("projectsDir");return std::string("/projects");};
  services.getConfigHome=[&]{logs.push_back("configHome");
    if(item.value("configError",false))throw std::runtime_error("config failed");
    return std::string("/config");};
  services.joinPath=shell::joinPosixPath;
  services.logError=[&](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& e){
      logs.push_back("log:"+std::string(e.what()));
    }
  };
  services.gadgetResultsSubdir="tool-results";
  services.fs.readdir=[&](const std::string& dir){
    logs.push_back("readdir:"+dir);
    event_loop::Deferred<std::vector<cleanup::Dirent>> result(loop);
    if(!dirs.contains(dir))result.reject(std::make_exception_ptr(
      std::system_error(std::make_error_code(std::errc::no_such_file_or_directory))));
    else{
      std::vector<cleanup::Dirent> entries;
      for(const auto& item:dirs[dir])entries.push_back({item.at("name").get<std::string>(),
        item.value("file",false),item.value("directory",false)});
      result.resolve(std::move(entries));
    }
    return result;
  };
  services.fs.statMtimeMs=[&](const std::string& path){
    logs.push_back("stat:"+path);
    event_loop::Deferred<double> result(loop);
    if(!mtimes.contains(path))result.reject(std::make_exception_ptr(
      std::system_error(std::make_error_code(std::errc::no_such_file_or_directory))));
    else result.resolve(*cleanup::indigenousLogDateMs(mtimes[path].get<std::string>()));
    return result;
  };
  services.fs.unlink=[&](const std::string& path){
    logs.push_back("unlink:"+path);
    event_loop::Deferred<event_loop::Unit> result(loop);
    if(contains(item.value("unlinkError",Json::array()),path))
      result.reject(std::make_exception_ptr(std::runtime_error("unlink "+path)));
    else result.resolve(event_loop::Unit{});
    return result;
  };
  services.fs.rmdir=[&](const std::string& path){
    logs.push_back("rmdir:"+path);
    event_loop::Deferred<event_loop::Unit> result(loop);result.resolve(event_loop::Unit{});return result;
  };
  services.fs.rmRecursiveForce=[&](const std::string& path){
    logs.push_back("rm:"+path+":true:true");
    event_loop::Deferred<event_loop::Unit> result(loop);result.resolve(event_loop::Unit{});return result;
  };
  const auto op=item.at("op").get<std::string>();
  event_loop::Deferred<cleanup::Result> pending=op=="cleanupOldSessionFiles"?
    cleanup::cleanupOldFlightFiles(loop,services):
    op=="cleanupOldPlanFiles"?cleanup::cleanupOldPlanFiles(loop,services):
    op=="cleanupOldFileHistoryBackups"?cleanup::cleanupOldFileHistoryBackups(loop,services):
    op=="cleanupOldSessionEnvDirs"?cleanup::cleanupOldFlightEnvDirs(loop,services):
    cleanup::cleanupOldDebugLogs(loop,services);
  Json value;bool finished=false;
  pending.then([&](const cleanup::Result* result,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
      value="ERROR:"+std::string(e.what());}}
    else value=Json{{"messages",result->messages},{"errors",result->errors}};
    finished=true;
  });
  loop.run();if(!finished)throw std::runtime_error("cleanup did not settle");
  return Json{{"name",item.at("name")},{"value",value},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);const std::string input(std::istreambuf_iterator<char>{stream},{});
  const auto cases=value::parse(input);Json out=Json::array();
  for(const auto& item:cases)out.push_back(evaluate(item));
  std::cout<<value::stringify(out)<<'\n';
}
