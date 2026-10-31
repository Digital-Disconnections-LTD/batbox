#include "cleanup/ExtraRetention.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace batbox;
namespace {
void write(const std::filesystem::path& path){
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);out<<"content";
}
void old(const std::filesystem::path& path){
  std::filesystem::last_write_time(path,
    std::filesystem::file_time_type::clock::now()-std::chrono::hours(24*100));
}
cleanup::Result await(event_loop::Loop& loop,event_loop::Deferred<cleanup::Result> pending){
  std::optional<cleanup::Result> result;
  pending.then([&](const cleanup::Result* value,const std::exception_ptr& error){
    assert(!error);result=*value;
  });
  loop.run();assert(result);return *result;
}
}
int main(){
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("batbox-extra-cleanup-"+std::to_string(::getpid()));
  const auto project=root/"projects"/"p1";
  const auto oldFlight=project/"old.jsonl",freshFlight=project/"fresh.jsonl";
  const auto oldGadget=project/"s1"/"tool-results"/"t1"/"out";
  const auto oldPlan=root/"config"/"plans"/"old.md";
  const auto history=root/"config"/"file-history"/"old";
  const auto env=root/"config"/"session-env"/"old";
  const auto debug=root/"config"/"debug"/"old.txt";
  write(oldFlight);write(freshFlight);write(oldGadget);write(oldPlan);
  write(history/"snapshot");write(env/"env");write(debug);
  for(const auto& path:{oldFlight,oldGadget,oldPlan,history,env,debug})old(path);
  event_loop::Loop loop;int diagnostics=0;
  auto services=cleanup::indigenousExtraServices(loop,
    []{const auto now=std::chrono::system_clock::now().time_since_epoch();
      return cleanup::cutoffDateMs(
        std::chrono::duration<double,std::milli>(now).count(),30);},
    [&]{return (root/"projects").string();},
    [&]{return (root/"config").string();},
    [&](std::exception_ptr){++diagnostics;},"tool-results");
  const auto flight=await(loop,cleanup::cleanupOldFlightFiles(loop,services));
  const auto plans=await(loop,cleanup::cleanupOldPlanFiles(loop,services));
  const auto histories=await(loop,cleanup::cleanupOldFileHistoryBackups(loop,services));
  const auto envs=await(loop,cleanup::cleanupOldFlightEnvDirs(loop,services));
  const auto debugs=await(loop,cleanup::cleanupOldDebugLogs(loop,services));
  assert(flight.messages==2&&flight.errors==0);
  assert(plans.messages==1&&histories.messages==1&&envs.messages==1&&debugs.messages==1);
  assert(diagnostics==0);
  assert(!fs::exists(oldFlight)&&!fs::exists(oldGadget)&&!fs::exists(oldPlan));
  assert(!fs::exists(history)&&!fs::exists(env)&&!fs::exists(debug));
  assert(fs::exists(freshFlight)&&fs::exists(root/"config"/"debug"));
  fs::remove_all(root);
}
