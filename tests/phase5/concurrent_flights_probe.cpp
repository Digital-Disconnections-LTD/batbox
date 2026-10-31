#include "flights/concurrent/Roster.hpp"
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> value){
 std::optional<T> result;std::exception_ptr failure;
 value.then([&](const T* v,const std::exception_ptr& e){failure=e;if(v)result=*v;});
 loop.run();if(failure)std::rethrow_exception(failure);
 if(!result)throw std::runtime_error("unsettled fixture promise");return *result;
}
Json run(const Json& item){
 event_loop::Loop loop;Json calls=Json::array(),results=Json::array();
 Json files=item.value("files",Json::object()),failures=item.value("failures",Json::object());
 Json state={{"id","s1"},{"cwd","/work"},{"now",123}};
 if(item.contains("state"))state.update(item["state"]);
 std::vector<std::function<event_loop::Deferred<event_loop::Unit>()>> cleanups;
 std::vector<std::function<void(const std::string&)>> switches;
 flights::concurrent::Services s;s.loop=&loop;s.pid=44;
 s.feature=[&](const std::string& key){for(const auto& f:item.value("features",Json::array()))if(f==key)return true;return false;};
 s.env=[&](const std::string& key)->std::optional<std::string>{
  const auto env=item.value("env",Json::object());if(!env.contains(key))return {};return env[key].get<std::string>();};
 s.getBatId=[&]()->std::optional<std::string>{if(!item.contains("agentId")||item["agentId"].is_null())return {};return item["agentId"].get<std::string>();};
 s.configHome=[&]{return item.value("home",std::string("/config"));};
 s.flightId=[&]{return state["id"].get<std::string>();};s.originalCwd=[&]{return state["cwd"].get<std::string>();};
 s.now=[&]{return state["now"].get<double>();};
 s.join=[](const std::string& a,const std::string& b){return a+"/"+b;};
 const auto unit=[&](const std::string& op,Json args){
  Json line=Json::array({op});for(const auto& v:args)line.push_back(v);calls.push_back(line);
  event_loop::Deferred<event_loop::Unit> out(loop);
  if(failures.contains(op)){out.reject(std::make_exception_ptr(std::runtime_error(failures[op].get<std::string>())));return out;}
  loop.queueMicrotask([&,out,op]{if(item.value("mutateAt",std::string())==op)state.update(item.at("mutation"));out.resolve({});});return out;
 };
 s.mkdir=[&](const std::string& p,bool r,int m){return unit("mkdir",Json::array({p,r,m}));};
 s.chmod=[&](const std::string& p,int m){return unit("chmod",Json::array({p,m}));};
 s.writeFile=[&](const std::string& p,const std::string& t){
  event_loop::Deferred<event_loop::Unit> out(loop);unit("write",Json::array({p,t})).then([&,out,p,t](const auto*,const std::exception_ptr& e){if(e)out.reject(e);else{files[p]=t;out.resolve({});}});return out;
 };
 s.readFile=[&](const std::string& p){event_loop::Deferred<std::string> out(loop);
  unit("read",Json::array({p,"utf8"})).then([&,out,p](const auto*,const std::exception_ptr& e){
   if(e)out.reject(e);else if(!files.contains(p))out.reject(std::make_exception_ptr(std::runtime_error("ENOENT")));
   else out.resolve(files[p].get<std::string>());
  });return out;
 };
 s.unlink=[&](const std::string& p){event_loop::Deferred<event_loop::Unit> out(loop);
  unit("unlink",Json::array({p})).then([&,out,p](const auto*,const std::exception_ptr& e){if(e)out.reject(e);else{files.erase(p);out.resolve({});}});return out;
 };
 s.readdir=[&](const std::string& p){event_loop::Deferred<std::vector<std::string>> out(loop);
  unit("readdir",Json::array({p})).then([&,out](const auto*,const std::exception_ptr& e){if(e)out.reject(e);else out.resolve(item.value("names",std::vector<std::string>{}));});return out;
 };
 s.jsonParse=[](const std::string& t){return value::parse(t);};s.jsonStringify=[](const Json& j){return value::stringify(j);};
 s.registerCleanup=[&](auto fn){calls.push_back(Json::array({"registerCleanup"}));cleanups.push_back(std::move(fn));};
 s.onFlightSwitch=[&](auto fn){calls.push_back(Json::array({"onSessionSwitch"}));switches.push_back(std::move(fn));};
 s.isProcessRunning=[&](double pid){calls.push_back(Json::array({"alive",pid}));for(const auto& v:item.value("alive",Json::array()))if(v==pid)return true;return false;};
 s.platform=[&]{calls.push_back(Json::array({"platform"}));return item.value("platform",std::string("linux"));};
 s.errorMessage=[](const std::exception_ptr& e){try{std::rethrow_exception(e);}catch(const std::exception& e){return std::string(e.what());}};
 s.isFsInaccessible=[&](const std::exception_ptr& e){const auto m=s.errorMessage(e);return m=="ENOENT"||m=="EACCES"||m=="EPERM"||m=="ENOTDIR"||m=="ELOOP";};
 s.debug=[&](const std::string& t){calls.push_back(Json::array({"debug",t}));};
 flights::concurrent::Roster roster(s);
 for(const auto& step:item["steps"]){try{
  Json result;const auto op=step["op"].get<std::string>();
  if(op=="register")result=await(loop,roster.registerFlight());
  else if(op=="bg")result=roster.isBgFlight();
  else if(op=="name"){std::optional<std::string> name;if(step.contains("value")&&!step["value"].is_null())name=step["value"].get<std::string>();await(loop,roster.updateFlightName(name));}
  else if(op=="bridge"){std::optional<std::string> id;if(!step["value"].is_null())id=step["value"].get<std::string>();await(loop,roster.updateFlightBridgeId(id));}
  else if(op=="activity")await(loop,roster.updateFlightActivity(step["value"]));
  else if(op=="count")result=await(loop,roster.countConcurrentFlights());
  else if(op=="switch")for(auto& fn:switches)fn(step["value"]);
  else if(op=="cleanup")for(auto& fn:cleanups)await(loop,fn());
  else if(op=="failures")failures=step["value"];
  else throw std::runtime_error("unknown op");
  loop.runReady();results.push_back(result);
 }catch(const std::exception& e){results.push_back(Json{{"error",e.what()}});}}
 return Json{{"results",results},{"calls",calls},{"files",files}};
}
}
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();for(const auto& item:input)out.push_back(run(item));std::cout<<value::stringify(out)<<'\n';}
