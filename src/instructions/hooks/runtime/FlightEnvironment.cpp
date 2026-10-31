#include "instructions/hooks/runtime/FlightEnvironmentInternal.hpp"
#include "shell/PosixPath.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
const std::regex hookFilePattern(
  R"(^(setup|sessionstart|cwdchanged|filechanged)-hook-([0-9]+)\.sh$)");
int priority(const std::string& kind){
  if(kind=="setup")return 0;
  if(kind=="sessionstart")return 1;
  if(kind=="cwdchanged")return 2;
  if(kind=="filechanged")return 3;
  return 99;
}
}
std::string joinFlightPath(const std::string& first,const std::string& second){
  return shell::joinPosixPath(first,second);
}
bool isHookEnvFile(const std::string& name){
  return std::regex_match(name,hookFilePattern);
}
bool isCwdEnvFile(const std::string& name){
  return (name.starts_with("filechanged-hook-")||
    name.starts_with("cwdchanged-hook-"))&&isHookEnvFile(name);
}
void sortHookEnvFiles(std::vector<std::string>& names){
  std::stable_sort(names.begin(),names.end(),[](const auto& a,const auto& b){
    std::smatch left,right;
    std::regex_match(a,left,hookFilePattern);
    std::regex_match(b,right,hookFilePattern);
    const auto leftPriority=priority(left.size()>1?left[1].str():"");
    const auto rightPriority=priority(right.size()>1?right[1].str():"");
    if(leftPriority!=rightPriority)return leftPriority<rightPriority;
    const auto leftIndex=value::parseInt10(left.size()>2?left[2].str():"0");
    const auto rightIndex=value::parseInt10(right.size()>2?right[2].str():"0");
    return leftIndex<rightIndex;
  });
}
FlightEnvironment::FlightEnvironment(event_loop::Loop& loop,
  FlightEnvironmentImports imports):state_(std::make_shared<State>(loop,
    std::move(imports))){
  const auto& services=state_->imports;
  if(!services.getConfigHomeDir||!services.getFlightId||!services.getPlatform||
    !services.getParentEnvFile||!services.mkdir||!services.readdir||
    !services.readFile||!services.writeFile||!services.logDebug||
    !services.errnoCode||!services.errorMessage)
    throw std::invalid_argument("Session environment imports required");
}
event_loop::Deferred<std::string> FlightEnvironment::getFlightEnvDirPath()const{
  auto state=state_;event_loop::Deferred<std::string> output(state->loop);
  try{
    const auto home=state->imports.getConfigHomeDir();
    const auto flight=state->imports.getFlightId();
    const auto path=joinFlightPath(joinFlightPath(home,"session-env"),flight);
    state->imports.mkdir(path).then([output,path](const event_loop::Unit*,
      const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve(path);
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<std::string> FlightEnvironment::getHookEnvFilePath(
  const std::string& hookEvent,std::size_t hookIndex)const{
  auto state=state_;event_loop::Deferred<std::string> output(state->loop);
  std::string prefix=hookEvent;
  for(auto& unit:prefix)unit=static_cast<char>(std::tolower(
    static_cast<unsigned char>(unit)));
  getFlightEnvDirPath().then([output,prefix,hookIndex](
    const std::string* path,const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    try{output.resolve(joinFlightPath(*path,prefix+"-hook-"+
      std::to_string(hookIndex)+".sh"));}
    catch(...){output.reject(std::current_exception());}
  });
  return output;
}
void FlightEnvironment::invalidateFlightEnvStash()const{
  auto state=state_;
  if(!state->loop.isOwnerThread())throw std::logic_error("Session env owner required");
  state->imports.logDebug("Invalidating session environment cache");
  state->script.reset();
}
} // namespace batbox::instructions::hooks::runtime
