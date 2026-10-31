#include "repository/Detect.hpp"
#include <stdexcept>
#include <unordered_map>

namespace batbox::repository {
namespace {
std::string errorText(std::exception_ptr error){
  try{std::rethrow_exception(error);}catch(const std::exception& e){return "Error: "+std::string(e.what());}
  catch(...){return "Unknown error";}
}
}
struct Detector::State {
  event_loop::Loop::Dispatcher owner;
  Services services;
  std::unordered_map<std::string,RepositoryRef> stash;
  State(event_loop::Loop& loop,Services svc):owner(loop.dispatcher()),services(std::move(svc)){}
  void check()const{if(!owner.isOwnerThread())throw std::logic_error("Repository detector requires owner loop");}
  void debug(const std::string& text)const{if(services.debug)services.debug(text);}
};
Detector::Detector(event_loop::Loop& loop,Services services):state_(std::make_shared<State>(loop,std::move(services))){}
void Detector::clearRepositoryStashes(){state_->check();state_->stash.clear();}
event_loop::Deferred<RepositoryRef> Detector::detectCurrentRepositoryWithHost(){
  state_->check();event_loop::Deferred<RepositoryRef> result(state_->owner);
  std::string cwd;
  try{
    if(!state_->services.getCwd)throw std::invalid_argument("Repository getCwd binding required");
    cwd=state_->services.getCwd();
    const auto found=state_->stash.find(cwd);
    if(found!=state_->stash.end()){result.resolve(found->second);return result;}
  }catch(...){result.reject(std::current_exception());return result;}
  auto fail=[state=state_,cwd,result](std::exception_ptr error){
    try{state->debug("Error detecting repository: "+errorText(error));state->stash[cwd]={};result.resolve({});}
    catch(...){result.reject(std::current_exception());}
  };
  try{
    if(!state_->services.getRemoteUrl)throw std::invalid_argument("Repository getRemoteUrl binding required");
    state_->services.getRemoteUrl().then([state=state_,cwd,result,fail](const std::optional<std::string>* remote,const std::exception_ptr& error){
      if(error){fail(error);return;}
      try{
        state->debug("Git remote URL: "+(*remote?**remote:"null"));
        if(!*remote||remote->value().empty()){
          state->debug("No git remote URL found");state->stash[cwd]={};result.resolve({});return;
        }
        auto parsed=parseGitRemote(**remote);
        state->debug("Parsed repository: "+(parsed?parsed->host+"/"+parsed->owner+"/"+parsed->name:"null")+" from URL: "+**remote);
        state->stash[cwd]=parsed;result.resolve(std::move(parsed));
      }catch(...){fail(std::current_exception());}
    });
  }catch(...){fail(std::current_exception());}
  return result;
}
event_loop::Deferred<std::optional<std::string>> Detector::detectCurrentRepository(){
  state_->check();event_loop::Deferred<std::optional<std::string>> result(state_->owner);
  detectCurrentRepositoryWithHost().then([result](const RepositoryRef* parsed,const std::exception_ptr& error){
    if(error)result.reject(error);
    else if(!*parsed||(*parsed)->host!="github.com")result.resolve(std::nullopt);
    else result.resolve((*parsed)->owner+"/"+(*parsed)->name);
  });return result;
}
std::optional<std::string> Detector::getStashedRepository()const{
  state_->check();
  if(!state_->services.getCwd)throw std::invalid_argument("Repository getCwd binding required");
  const auto found=state_->stash.find(state_->services.getCwd());
  if(found==state_->stash.end()||!found->second||found->second->host!="github.com")return std::nullopt;
  return found->second->owner+"/"+found->second->name;
}
}
