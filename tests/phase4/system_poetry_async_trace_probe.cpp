#include "PoetryAsyncFixture.hpp"
#include <iostream>
#include <map>
#include <variant>
using namespace batbox;
namespace {
using Pending=std::variant<event_loop::Deferred<poetryEntries::PoetryObject>,event_loop::Deferred<std::string>,
  event_loop::Deferred<poetryEntries::SectionValue>>;
Json run(Json input){
 event_loop::Loop loop;Json events=Json::array();poetryEntries::AsyncSystemPoetryServices services;
 poetry_fixture::bind(loop,services,input,events);
 services.getCwd=[&]{events.push_back("cwd");return "/project";};
 services.internalUser=input.value("internal",false);
 if(input.value("simple",false))services.environment["BATBOX_SIMPLE"]="true";
 services.sections=std::make_shared<poetryEntries::AsyncSectionStash>(loop);
 std::map<std::string,std::vector<Pending>> pending;
 auto settings=std::make_shared<Json>(Json::object());
 if(input.contains("language"))(*settings)["language"]=input["language"];
 auto styleObject=std::make_shared<Json>(input.value("style",Json(nullptr)));
 services.getInitialSettings=[&]{events.push_back("settings");return settings;};
 auto imported=[&]<class T>(const std::string& key,T value){
  events.push_back(key);
  if(value::property(input,"syncError")==key)throw std::runtime_error(key+" failed");
  bool delayed=false;for(const auto& d:value::property(input,"delays"))if(d==key)delayed=true;
  event_loop::Deferred<T> result(loop);
  if(delayed)pending[key].emplace_back(result);else result.resolve(std::move(value));
  return result;
 };
 services.getSkillGadgetCommands=[&](const std::string&){return imported("skills",std::make_shared<Json>(Json::array()));};
 services.getOutputStyleConfig=[&]{return imported("style",styleObject);};
 services.computeSimpleEnvInfo=[&](const std::string&,const auto&){return imported("env",std::string("ENV"));};
 services.loadMemoryPoetry=[&]{return imported("memory",poetryEntries::SectionValue("MEMORY"));};
 poetryEntries::AsyncSystemPoetryInput args;args.model="local";
 *args.gadgets=input.value("tools",Json::array());*args.mcpClients=input.value("mcpClients",Json::array());
 if(input.contains("additional"))*args.additionalWorkingDirectories=input["additional"].get<std::vector<std::string>>();
 const auto actions=input.at("actions");
 for(const auto& a:actions){
  const std::string op=a.at("op");
  if(op=="start"){
   const std::string id=a.at("id");
   poetryEntries::getSystemPoetryAsync(loop,args,services).then([&,id](const std::vector<std::string>* result,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){events.push_back({{"id",id},{"error",e.what()}});}}
    else events.push_back({{"id",id},{"result",*result}});
   });events.push_back("returned:"+id);
  }else if(op=="resolve"||op=="reject"){
   const std::string key=a.at("key");const int index=a.value("index",0);
   std::visit([&](const auto& promise){
    if(op=="reject"){promise.reject(std::make_exception_ptr(std::runtime_error(key+" failed")));return;}
    using P=std::decay_t<decltype(promise)>;
    if constexpr(std::is_same_v<P,event_loop::Deferred<poetryEntries::PoetryObject>>)promise.resolve(std::make_shared<Json>(a.at("value")));
    else if constexpr(std::is_same_v<P,event_loop::Deferred<std::string>>)promise.resolve(a.at("value").get<std::string>());
    else promise.resolve(a.at("value").is_null()?poetryEntries::SectionValue{}:poetryEntries::SectionValue(a.at("value").get<std::string>()));
   },pending.at(key).at(index));
   events.push_back("settled:"+key+":"+std::to_string(index));
  }else if(op=="flush"){loop.runReady();events.push_back("flushed");}
  else if(op=="tool")args.gadgets->push_back({{"name",a.at("name")}});
  else if(op=="client")args.mcpClients->push_back(a.at("value"));
  else if(op=="state"){
   const std::string key=a.at("key");input[key]=a.at("value");if(key=="language")(*settings)[key]=a.at("value");
  }
  else if(op=="setting")(*settings)[a.at("key").get<std::string>()]=a.at("value");
  else if(op=="styleField")(*styleObject)[a.at("key").get<std::string>()]=a.at("value");
  else if(op=="clear"){services.sections->clearSystemPoetrySections([]{});events.push_back("cleared");}
 }
 loop.runReady();return events;
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}}
