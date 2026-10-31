#include "context/AsyncContext.hpp"
#include <iostream>
#include <map>
#include <variant>
using namespace batbox;
namespace {
using Text=event_loop::Deferred<std::string>;
using Git=event_loop::Deferred<std::optional<std::string>>;
using Object=event_loop::Deferred<Json>;
using Files=event_loop::Deferred<context::MemoryFiles>;
using Boolean=event_loop::Deferred<bool>;
using Pending=std::variant<Boolean,Text,Files>;
using Promise=std::variant<Git,Object>;
bool flag(const Json& input,const char* key){return value::truthy(value::property(input,key));}
template<class T>void observe(const event_loop::Deferred<T>& promise,const std::string& id,Json& trace){
  promise.then([&trace,id](const T* value,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){trace.push_back({{"id",id},{"error",e.what()}});}return;}
    if constexpr(std::is_same_v<T,std::optional<std::string>>)
      trace.push_back({{"id",id},{"result",*value?Json(**value):Json(nullptr)}});
    else trace.push_back({{"id",id},{"result",*value}});
  });
}
Json run(const Json& input){
  event_loop::Loop loop;context::AsyncContextStash stash(loop);
  Json trace=Json::array();std::map<std::string,std::vector<Pending>> pending;
  std::map<std::string,Promise> promises;context::AsyncServices services;
  auto imported=[&]<class T>(const std::string& key){
    trace.push_back("call:"+key);
    if(value::property(input,"syncError")==key)throw std::runtime_error(key+" failed");
    event_loop::Deferred<T> result(loop);pending[key].emplace_back(result);return result;
  };
  services.testEnvironment=flag(input,"test");
  if(input.contains("remote"))services.environment["BATBOX_REMOTE"]=input["remote"];
  if(input.contains("disable"))services.environment["BATBOX_DISABLE_BATBOX_MDS"]=input["disable"];
  services.getIsGit=[&]{return imported.operator()<bool>("isGit");};
  services.getBranch=[&]{return imported.operator()<std::string>("branch");};
  services.getDefaultBranch=[&]{return imported.operator()<std::string>("main");};
  services.gitStdoutNoThrow=[&](const std::vector<std::string>& args){
    return imported.operator()<std::string>(args[1]=="status"?"status":args[1]=="log"?"log":"name");
  };
  services.includeGitInstructions=[&]{trace.push_back("includeGit");return value::property(input,"includeGit")!=false;};
  services.stashBreakerFeature=[&]{trace.push_back("feature:BREAK_CACHE_COMMAND");return flag(input,"feature");};
  services.bareMode=[&]{trace.push_back("bare");return flag(input,"bare");};
  services.additionalInstructionDirectories=[&]{trace.push_back("additional");
    return input.contains("additional")?input["additional"].get<std::vector<std::string>>():std::vector<std::string>{};};
  services.getMemoryFiles=[&]{return imported.operator()<context::MemoryFiles>("files");};
  instructions::project::DiscoveryServices project;
  project.skipAutoMemoryIndex=project.skipProjectInstructions=[]{return false;};
  project.teamMemoryFeature=[&]{trace.push_back("feature:TEAMMEM");return false;};
  services.projectServices=&project;
  services.setStashedBatboxMdContent=[&](const std::optional<std::string>& text){
    trace.push_back({{"cachedContent",text?Json(*text):Json(nullptr)}});};
  services.localISODate=[&]{trace.push_back("date");return "2026-10-28";};
  services.logError=[&](std::exception_ptr){trace.push_back("logError");if(flag(input,"logErrorThrows"))throw std::runtime_error("log failed");};
  for(const auto& action:input.at("actions")){
    const std::string op=action.at("op");
    if(op=="start"){
      const std::string kind=action.at("kind"),id=action.at("id");
      if(kind=="git"){
        auto promise=stash.getGitStatus(services);promises.emplace(id,promise);observe(promise,id,trace);
      }else{
        auto promise=kind=="system"?stash.getSystemContext(services):stash.getUserContext(services);
        promises.emplace(id,promise);observe(promise,id,trace);
      }
      trace.push_back("returned:"+id);
    }else if(op=="same"){
      const auto& a=promises.at(action.at("a"));const auto& b=promises.at(action.at("b"));
      bool same=false;
      if(a.index()==b.index())same=std::visit([&](const auto& x){
        using P=std::decay_t<decltype(x)>;return x.samePromise(std::get<P>(b));},a);
      trace.push_back({{"same",same}});
    }else if(op=="flush"){loop.runReady();trace.push_back("flushed");}
    else if(op=="resolve"||op=="reject"){
      const std::string key=action.at("key");const auto index=action.value("index",0);
      auto& p=pending.at(key).at(index);
      std::visit([&](const auto& promise){
        using P=std::decay_t<decltype(promise)>;
        if(op=="reject"){promise.reject(std::make_exception_ptr(std::runtime_error(key+" failed")));return;}
        const auto& value=action.at("value");
        if constexpr(std::is_same_v<P,Boolean>)promise.resolve(value.get<bool>());
        else if constexpr(std::is_same_v<P,Text>)promise.resolve(value.get<std::string>());
        else{
          context::MemoryFiles files;
          for(const auto& item:value){instructions::project::MemoryFileInfo file;
            file.path=item.at("path");file.type=item.at("type");file.content=item.at("content");files.push_back(std::move(file));}
          promise.resolve(std::move(files));
        }
      },p);
      trace.push_back("settled:"+key+":"+std::to_string(index));
    }else if(op=="inject"){
      stash.setSystemPoetryInjection(action.contains("value")&&!action["value"].is_null()
        ?std::optional<std::string>(action["value"].get<std::string>()):std::nullopt);
    }else if(op=="clear"){
      const std::string kind=action.at("kind");
      if(kind=="git")stash.clearGitStatusStash();else if(kind=="system")stash.clearSystemContextStash();else stash.clearUserContextStash();
    }
  }
  loop.runReady();return trace;
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
