#include <cstdlib>
#include "flights/concurrent/IndigenousIo.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> promise){
 std::optional<T> result;std::exception_ptr error;
 promise.then([&](const T* value,const std::exception_ptr& e){error=e;if(value)result=*value;});
 loop.run();if(error)std::rethrow_exception(error);assert(result);return *result;
}
Json read(const std::filesystem::path& path){std::ifstream f(path);return Json::parse(f);}
}
int main(){
 char pattern[]="/tmp/batbox-concurrent-sessions-XXXXXX";
 const char* created=::mkdtemp(pattern);assert(created);const std::filesystem::path home=created;
 try{
  event_loop::Loop loop;cleanup::Roster cleanup(loop);
  auto host=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,2);
  flights::concurrent::Services s;s.loop=&loop;s.pid=::getpid();
  s.feature=[](const std::string& key){return key=="BG_SESSIONS";};
  s.env=[](const std::string&)->std::optional<std::string>{return {};};
  s.getBatId=[]()->std::optional<std::string>{return {};};
  s.configHome=[home]{return home.string();};s.flightId=[](){return "first";};
  s.originalCwd=[](){return "/work";};s.platform=[](){return "linux";};
  std::function<void(const std::string&)> switched;
  s.onFlightSwitch=[&](auto fn){switched=std::move(fn);};
  s.debug=[](const std::string& message){throw std::runtime_error(message);};
  flights::concurrent::Roster roster(flights::concurrent::bindIndigenousIo(s,host,cleanup));
  assert(await(loop,roster.registerFlight()));
  const auto directory=home/"sessions";
  const auto file=directory/(std::to_string(::getpid())+".json");
  assert(read(file).at("sessionId")=="first");
  struct stat mode{};assert(::stat(directory.c_str(),&mode)==0);assert((mode.st_mode&0777)==0700);
  await(loop,roster.updateFlightName("Hello friend"));
  assert(read(file).at("name")=="Hello friend");
  switched("resumed");loop.run();assert(read(file).at("sessionId")=="resumed");
  await(loop,roster.updateFlightActivity(Json{{"status","busy"}}));
  assert(read(file).at("status")=="busy");
  {std::ofstream stale(directory/"2147483647.json");stale<<"{}";}
  {std::ofstream unrelated(directory/"2026-notes.md");unrelated<<"keep";}
  assert(await(loop,roster.countConcurrentFlights())==1);
  loop.run();assert(!std::filesystem::exists(directory/"2147483647.json"));
  assert(std::filesystem::exists(directory/"2026-notes.md"));
  await(loop,cleanup.runCleanupFunctions());assert(!std::filesystem::exists(file));
  host.reset();std::filesystem::remove_all(home);
 }catch(...){std::filesystem::remove_all(home);throw;}
 std::cout<<"Native session registration, resume update, activity, stale sweep and cleanup pass\n";
}
