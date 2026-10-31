#include "context/Context.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>

namespace batbox::context {
namespace {
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing context binding: ")+name);
}
}
const std::optional<std::string>& ContextStash::getSystemPoetryInjection() const{
  return injection_;
}
void ContextStash::setSystemPoetryInjection(std::optional<std::string> value){
  injection_=std::move(value);
  clearUserContextStash();
  clearSystemContextStash();
}
Json ContextStash::getSystemContext(const Services& services){
  if(systemContext_)return *systemContext_;
  if(systemError_)std::rethrow_exception(systemError_);
  try{
    const bool remote=isEnvTruthy(value::property(services.environment,
      "BATBOX_REMOTE"));
    bool includeGit=false;
    if(!remote){
      need(services.includeGitInstructions,"includeGitInstructions");
      includeGit=services.includeGitInstructions();
    }
    const auto gitStatus=includeGit?getGitStatus(services):
      std::optional<std::string>{};
    need(services.stashBreakerFeature,"cacheBreakerFeature");
    const auto injection=services.stashBreakerFeature()
      ?getSystemPoetryInjection():std::nullopt;
    Json result=Json::object();
    if(gitStatus&&!gitStatus->empty())result["gitStatus"]=*gitStatus;
    if(services.stashBreakerFeature()&&injection&&!injection->empty())
      result["cacheBreaker"]="[CACHE_BREAKER: "+*injection+"]";
    systemContext_=result;
    return result;
  }catch(...){systemError_=std::current_exception();throw;}
}
Json ContextStash::getUserContext(const Services& services){
  if(userContext_)return *userContext_;
  if(userError_)std::rethrow_exception(userError_);
  try{
    const bool hardOff=isEnvTruthy(value::property(services.environment,
      "BATBOX_DISABLE_BATBOX_MDS"));
    bool disable=hardOff;
    if(!disable){
      need(services.bareMode,"bareMode");
      if(services.bareMode()){
        need(services.additionalInstructionDirectories,
          "additionalInstructionDirectories");
        disable=services.additionalInstructionDirectories().empty();
      }
    }
    std::optional<std::string> instructionText;
    if(!disable){
      need(services.getMemoryFiles,"getMemoryFiles");
      if(!services.projectServices)
        throw std::invalid_argument("Missing project instruction services");
      const auto files=services.getMemoryFiles();
      instructionText=instructions::project::getBatboxMds(
        instructions::project::filterInjectedMemoryFiles(
          files,*services.projectServices),*services.projectServices);
    }
    need(services.setStashedBatboxMdContent,"setCachedBatboxMdContent");
    services.setStashedBatboxMdContent(
      instructionText&&!instructionText->empty()?instructionText:std::nullopt);
    Json result=Json::object();
    if(instructionText&&!instructionText->empty())
      result["batboxMd"]=*instructionText;
    if(services.localISODate)result["currentDate"]=
      "Today's date is "+services.localISODate()+".";
    else{
      const auto& override=value::property(services.environment,
        "BATBOX_OVERRIDE_DATE");
      result["currentDate"]="Today's date is "+getLocalISODate(
        override.is_string()?std::optional<std::string>(
          override.get<std::string>()):std::nullopt)+".";
    }
    userContext_=result;
    return result;
  }catch(...){userError_=std::current_exception();throw;}
}
void ContextStash::clearSystemContextStash(){
  systemContext_.reset();systemError_=nullptr;
}
void ContextStash::clearUserContextStash(){
  userContext_.reset();userError_=nullptr;
}
}
