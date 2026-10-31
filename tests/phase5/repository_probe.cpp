#include "repository/Detect.hpp"
#include <iostream>
#include <map>
using namespace batbox;
namespace {
Json run(const Json& input){
 event_loop::Loop loop;Json events=Json::array();
 std::string cwd="/a";std::vector<event_loop::Deferred<std::optional<std::string>>> remote;
 std::vector<event_loop::Deferred<repository::ParseGitRemote>> imports;
 std::map<std::string,repository::RepositoryRef> objects;
 auto debug=[&](const std::string& message){events.push_back("debug:"+message);if(input.value("debugThrow",false))throw std::runtime_error("debug failed");};
 const auto op=input.value("op",std::string("parse"));
 if(op!="sequence"){
  const auto value=input.at("value").get<std::string>();Json result;
  if(op=="hostname")result=repository::looksLikeRealHostname(value);
  else if(op=="github"){const auto p=repository::parseGitHubRepository(value,debug);result=p?Json(*p):Json(nullptr);}
  else{const auto p=repository::parseGitRemote(value);result=p?p->toJson():Json(nullptr);}
  return {{"result",result},{"events",events}};
 }
 repository::Services services;
 services.debug=debug;
 services.getCwd=[&]{events.push_back("cwd:"+cwd);if(input.value("cwdError",false))throw std::runtime_error("cwd failed");return cwd;};
 services.getRemoteUrl=[&]{
  events.push_back("remote");if(input.value("remoteThrow",false))throw std::runtime_error("remote failed");
  event_loop::Deferred<std::optional<std::string>> pending(loop);
  if(input.value("delayedRemote",false))remote.push_back(pending);
  else pending.resolve(input.contains("remote")&&!input.at("remote").is_null()?std::optional<std::string>(input.at("remote").get<std::string>()):std::nullopt);
  return pending;
 };
 repository::ParserImport loadParser=[&]{
  events.push_back("import");event_loop::Deferred<repository::ParseGitRemote> pending(loop);
  if(input.value("delayedParser",false))imports.push_back(pending);else pending.resolve(repository::parseGitRemote);
  return pending;
 };
 repository::Detector detector(loop,services);
 for(const auto& a:input.at("actions")){
  const std::string kind=a.at("op");
  if(kind=="start"){
   const std::string id=a.at("id");
   auto failure=[&,id](std::exception_ptr error){try{std::rethrow_exception(error);}catch(const std::exception& e){events.push_back({{"id",id},{"error",e.what()}});}};
   if(a.at("kind")=="withHost")detector.detectCurrentRepositoryWithHost().then(
    [&,id,failure](const repository::RepositoryRef* value,const std::exception_ptr& error){
      if(error){failure(error);return;}objects[id]=*value;events.push_back({{"id",id},{"result",*value?(*value)->toJson():Json(nullptr)}});
    });
   else{
    auto pending=a.at("kind")=="github"?repository::getGithubRepo(loop,loadParser,services.getRemoteUrl,debug):detector.detectCurrentRepository();
    pending.then([&,id,failure](const std::optional<std::string>* value,const std::exception_ptr& error){
      if(error){failure(error);return;}events.push_back({{"id",id},{"result",*value?Json(**value):Json(nullptr)}});
    });
   }
   events.push_back("returned:"+id);
  }else if(kind=="resolve"||kind=="reject"){
   const std::string key=a.at("key");const auto index=a.value("index",0);
   if(key=="parser"){
    if(kind=="reject")imports.at(index).reject(std::make_exception_ptr(std::runtime_error("parser failed")));
    else imports.at(index).resolve(repository::parseGitRemote);
   }else{
    if(kind=="reject")remote.at(index).reject(std::make_exception_ptr(std::runtime_error("remote failed")));
    else remote.at(index).resolve(a.at("value").is_null()?std::nullopt:std::optional<std::string>(a.at("value").get<std::string>()));
   }
   events.push_back("settled:"+key+":"+std::to_string(index));
  }else if(kind=="flush"){loop.runReady();events.push_back("flushed");}
  else if(kind=="cwd")cwd=a.at("value");
  else if(kind=="clear"){detector.clearRepositoryStashes();events.push_back("cleared");}
  else if(kind=="cached"){
   try{const auto p=detector.getStashedRepository();events.push_back({{"cached",p?Json(*p):Json(nullptr)}});}
   catch(const std::exception& e){events.push_back({{"cachedError",e.what()}});}
  }else if(kind=="same")events.push_back({{"same",objects.at(a.at("a"))==objects.at(a.at("b"))}});
  else if(kind=="mutate"){
   const auto p=objects.at(a.at("id"));const std::string key=a.at("key"),value=a.at("value");
   if(key=="owner")p->owner=value;else if(key=="name")p->name=value;else p->host=value;
  }
 }
 loop.runReady();return events;
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}}
