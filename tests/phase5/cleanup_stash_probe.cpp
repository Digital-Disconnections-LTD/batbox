#include "cleanup/StashThrottles.hpp"
#include "runtime/JsonValue.hpp"
#include "shell/PosixPath.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace batbox;
namespace {
std::string message(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}catch(const std::exception& e){return e.what();}
}
Json run(const Json& item){
  event_loop::Loop loop;Json logs=Json::array();
  cleanup::StashThrottleServices services;
  services.configHome=[&]{logs.push_back("configHome");return std::string("/config");};
  services.homeDir=[&]{logs.push_back("homeDir");return std::string("/home");};
  services.nowMs=[]{return 1000000000.0;};
  services.nowIso=[]{return std::string("2026-10-28T12:00:00.000Z");};
  services.joinPath=shell::joinPosixPath;
  services.packageKeySubstring="@batbox/batbox-";
  services.packageDebugLabel="@batbox";
  services.debug=[&](const std::string& value){logs.push_back("debug:"+value);};
  services.logError=[&](std::exception_ptr error){logs.push_back("error:"+message(error));};
  services.statMtimeMs=[&](const std::string& marker){
    logs.push_back("stat:"+marker);
    event_loop::Deferred<double> output(loop);
    if(item.contains("markerMtime"))output.resolve(item["markerMtime"].get<double>());
    else output.reject(std::make_exception_ptr(std::runtime_error("missing marker")));
    return output;
  };
  services.lock=[&](const std::string& marker){
    logs.push_back("lock:"+marker);
    event_loop::Deferred<event_loop::Unit> output(loop);
    if(item.value("lockHeld",false))output.reject(std::make_exception_ptr(std::runtime_error("held")));
    else output.resolve(event_loop::Unit{});
    return output;
  };
  services.unlock=[&](const std::string& marker){
    logs.push_back("unlock:"+marker);
    event_loop::Deferred<event_loop::Unit> output(loop);output.resolve(event_loop::Unit{});return output;
  };
  services.writeFile=[&](const std::string& marker,const std::string& value){
    logs.push_back("write:"+marker+":"+value);
    event_loop::Deferred<event_loop::Unit> output(loop);output.resolve(event_loop::Unit{});return output;
  };
  services.loadStashApi=[&]{
    logs.push_back("loadCache");
    event_loop::Deferred<cleanup::StashApi> output(loop);
    cleanup::StashApi api;
    api.listStream=[&](const std::string& path){
      logs.push_back("stream:"+path);
      const auto entries=item.value("entries",Json::array());
      auto index=std::make_shared<std::size_t>(0);
      cleanup::StashEntryStream stream;
      stream.next=[&,entries,index]{
        event_loop::Deferred<std::optional<cleanup::StashEntry>> next(loop);
        if(*index>=entries.size())next.resolve(std::nullopt);
        else{
          const auto& entry=entries[(*index)++];
          const auto key=entry.at("key").get<std::string>();
          logs.push_back("next:"+key);
          next.resolve(cleanup::StashEntry{key,entry.at("time").get<double>()});
        }
        return next;
      };
      return stream;
    };
    api.removeEntry=[&](const std::string&,const std::string& key){
      logs.push_back("remove:"+key);
      event_loop::Deferred<event_loop::Unit> result(loop);
      if(item.value("removeError",false))result.reject(
        std::make_exception_ptr(std::runtime_error("remove failed")));
      else result.resolve(event_loop::Unit{});
      return result;
    };
    output.resolve(api);return output;
  };
  services.cleanupOldVersions=[&]{
    logs.push_back("cleanupVersions");
    event_loop::Deferred<event_loop::Unit> result(loop);
    if(item.value("versionError",false))result.reject(
      std::make_exception_ptr(std::runtime_error("versions failed")));
    else result.resolve(event_loop::Unit{});
    return result;
  };
  auto pending=item.at("op")=="package"?
    cleanup::cleanupPackageStashThrottled(loop,services):
    cleanup::cleanupOldVersionsThrottled(loop,services);
  bool completed=false;
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);completed=true;
  });
  loop.run();if(!completed)throw std::runtime_error("cache cleanup did not complete");
  return Json{{"name",item.at("name")},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);const std::string input(std::istreambuf_iterator<char>{stream},{});
  const auto cases=value::parse(input);Json out=Json::array();
  for(const auto& item:cases)out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
