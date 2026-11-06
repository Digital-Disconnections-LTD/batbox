#include "application/HostRetention.hpp"
#include "cleanup/Background.hpp"
#include "cleanup/ExtraRetention.hpp"
#include "gadget_results/Storage.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include "settings/schema/IndigenousBindings.hpp"
#include <cerrno>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace batbox::application {
namespace {
using event_loop::Deferred;
using event_loop::Unit;
std::string processId(){
#ifdef _WIN32
  return std::to_string(::GetCurrentProcessId());
#else
  return std::to_string(::getpid());
#endif
}
bool processRunning(const std::string& name){
  long long pid=0;
  try{std::size_t used=0;pid=std::stoll(name,&used);if(used!=name.size())return false;}
  catch(...){return false;}
  if(pid<=1)return false;
#ifdef _WIN32
  HANDLE process=::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,static_cast<DWORD>(pid));
  if(!process)return ::GetLastError()==ERROR_ACCESS_DENIED;
  DWORD code=0;const bool okay=::GetExitCodeProcess(process,&code)!=0;
  ::CloseHandle(process);
  return okay&&code==STILL_ACTIVE;
#else
  return ::kill(static_cast<pid_t>(pid),0)==0||errno==EPERM;
#endif
}
std::filesystem::path runningDirectory(const std::filesystem::path& state){
  return state/"running";
}
// A path belongs to a Flight when any part of it, extension dropped, is the id:
// projects/<p>/<id>.jsonl, projects/<p>/<id>/tool-results, file-history/<id>,
// session-env/<id>, image-cache/<id>, debug/<id>.txt.
bool belongsTo(const std::string& path,const std::set<std::string>& flights){
  for(const auto& part:runtime::pathFromUtf8(path)){
    const auto name=runtime::pathToUtf8(part.filename());
    if(name.empty())continue;
    if(flights.contains(name))return true;
    const auto stem=runtime::pathToUtf8(part.stem());
    if(!stem.empty()&&flights.contains(stem))return true;
  }
  return false;
}
struct Protected {
  std::set<std::string> held;
  std::function<std::string()> current;
  bool covers(const std::string& path)const{
    auto flights=held;
    if(current){const auto id=current();if(!id.empty())flights.insert(id);}
    return belongsTo(path,flights);
  }
};
template<class Remove>Remove guard(event_loop::Loop& owner,Remove remove,
    std::shared_ptr<const Protected> keep){
  return [&owner,remove=std::move(remove),keep](const std::string& path){
    if(keep->covers(path)){Deferred<Unit> kept(owner);kept.resolve({});return kept;}
    return remove(path);};
}
}
void recordRunningFlight(const std::filesystem::path& stateDirectory,const std::string& flightId){
  std::error_code error;
  const auto directory=runningDirectory(stateDirectory);
  std::filesystem::create_directories(directory,error);
  if(error)return;
  std::ofstream(directory/processId(),std::ios::trunc)<<flightId<<'\n';
}
void forgetRunningFlight(const std::filesystem::path& stateDirectory){
  std::error_code error;
  std::filesystem::remove(runningDirectory(stateDirectory)/processId(),error);
}
std::set<std::string> heldFlights(const std::filesystem::path& stateDirectory){
  std::set<std::string> held;
  std::error_code error;
  for(std::filesystem::directory_iterator it(runningDirectory(stateDirectory),error),end;
      !error&&it!=end;it.increment(error)){
    const auto name=runtime::pathToUtf8(it->path().filename());
    if(!processRunning(name)){std::error_code ignored;
      std::filesystem::remove(it->path(),ignored);continue;}
    std::ifstream file(it->path());
    for(std::string line;std::getline(file,line);)if(!line.empty())held.insert(line);
  }
  return held;
}
Deferred<Unit> runRetention(event_loop::Loop& owner,RetentionInputs inputs){
  const auto& configured=value::property(inputs.settings,"cleanupPeriodDays");
  std::optional<double> days;
  if(configured.is_number())days=configured.get<double>();
  const double cutoff=cleanup::cutoffDateMs(static_cast<double>(platform::nowMillis()),days);
  auto keep=std::make_shared<Protected>();
  keep->held=heldFlights(inputs.stateDirectory);
  keep->current=inputs.currentFlight;
  const auto state=runtime::pathToUtf8(inputs.stateDirectory);
  auto log=inputs.log;
  const auto logError=[log](std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception& failure){log(std::string("Retention cleanup: ")+failure.what());}
    catch(...){log("Retention cleanup failed");}};
  auto extra=cleanup::indigenousExtraServices(owner,[cutoff]{return cutoff;},
    [state]{return runtime::pathToUtf8(runtime::pathFromUtf8(state)/"projects");},
    [state]{return state;},logError,gadget_results::GADGET_RESULTS_SUBDIR);
  const std::shared_ptr<const Protected> kept=keep;
  extra.fs.unlink=guard(owner,extra.fs.unlink,kept);
  extra.fs.rmdir=guard(owner,extra.fs.rmdir,kept);
  extra.fs.rmRecursiveForce=guard(owner,extra.fs.rmRecursiveForce,kept);
  const auto validate=settings::schema::bindIndigenousSettingsSchema(
    settings::schema::MainConfig{},settings::PermissionRuleServices{});
  cleanup::BackgroundServices services;
  services.settingsErrorCount=[validate,settings=inputs.settings]()->std::size_t{
    try{return validate(settings).success?0:1;}catch(...){return 1;}};
  services.rawSettingsContainsKey=[settings=inputs.settings](const std::string& key){
    return settings.is_object()&&settings.contains(key);};
  services.debug=log;
  // Batbox keeps no message caches; this step sweeps background shell output.
  services.oldMessageFiles=[&owner,extra,shellOutput=state+"/shell-output"]{
    Deferred<cleanup::Result> result(owner);
    cleanup::cleanupSingleDirectory(owner,shellOutput,".output",false,extra).then(
      [&owner,result,extra,shellOutput](const cleanup::Result* outputs,
          const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        const auto first=*outputs;
        cleanup::cleanupSingleDirectory(owner,shellOutput,"-cwd",false,extra).then(
          [result,first](const cleanup::Result* cwds,const std::exception_ptr& failure){
            if(failure)result.reject(failure);
            else result.resolve(cleanup::addCleanupResults(first,*cwds));});
      });
    return result;};
  services.oldFlightFiles=[&owner,extra]{return cleanup::cleanupOldFlightFiles(owner,extra);};
  services.oldPlanFiles=[&owner,extra]{return cleanup::cleanupOldPlanFiles(owner,extra);};
  services.oldFileHistoryBackups=[&owner,extra]{
    return cleanup::cleanupOldFileHistoryBackups(owner,extra);};
  services.oldFlightEnvDirs=[&owner,extra]{return cleanup::cleanupOldFlightEnvDirs(owner,extra);};
  services.oldDebugLogs=[&owner,extra]{return cleanup::cleanupOldDebugLogs(owner,extra);};
  services.oldImageStashes=[sweep=inputs.oldImageStashes,cutoff,keep]{
    auto held=keep->held;
    if(keep->current){const auto id=keep->current();if(!id.empty())held.insert(id);}
    return sweep(cutoff,std::move(held));};
  services.oldPastes=inputs.oldPastes;
  // Bat worktrees can hold uncommitted work, so age alone never removes them.
  services.staleBatWorktrees=[&owner](double){
    Deferred<int> none(owner);none.resolve(0);return none;};
  services.getCutoffDateMs=[cutoff]{return cutoff;};
  return cleanup::cleanupOldMessageFilesInBackground(owner,services);
}
}
