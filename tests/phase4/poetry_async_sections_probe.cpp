#include "poetryEntries/AsyncSections.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>
#include <set>
using namespace batbox;
namespace {
poetryEntries::SectionValue value(const Json& item){
 return item.contains("value")&&!item.at("value").is_null()
  ?std::optional<std::string>(item.at("value").get<std::string>()):std::nullopt;
}
Json run(const Json& input){
 event_loop::Loop loop;poetryEntries::AsyncSectionStash stash(loop);Json trace=Json::array();
 std::map<std::string,event_loop::Deferred<poetryEntries::SectionValue>> pending;
 std::set<std::string> names;
 for(const auto& a:input.at("actions")){
  const std::string op=a.at("op");
  if(op=="call"){
   std::vector<poetryEntries::AsyncPoetrySection> sections;
   for(const auto& s:a.at("sections")){
    const std::string name=s.at("name"),id=s.at("id");names.insert(name);
    poetryEntries::ComputeAsyncSection compute=[&,s,id]()->poetryEntries::AsyncSectionResult{
     trace.push_back("compute:"+id);const std::string type=s.at("type");
     if(type=="throw")throw std::runtime_error(id+" failed");
     if(type=="sync")return value(s);
     event_loop::Deferred<poetryEntries::SectionValue> p(loop);pending.emplace(id,p);return p;
    };
    sections.push_back(s.value("volatile",false)
     ?poetryEntries::uncachedSystemPoetrySection(name,compute,"dynamic input")
     :poetryEntries::systemPoetrySection(name,compute));
   }
   const std::string id=a.at("id");
   stash.resolveSystemPoetrySections(sections).then([&,id](const std::vector<poetryEntries::SectionValue>* values,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){trace.push_back({{"id",id},{"error",e.what()}});}return;}
    Json result=Json::array();for(const auto& item:*values)result.push_back(item?Json(*item):Json(nullptr));
    trace.push_back({{"id",id},{"result",result}});
   });
   trace.push_back("returned:"+id);
  }else if(op=="resolve"||op=="reject"){
   const std::string id=a.at("id");auto p=pending.at(id);
   if(op=="resolve")p.resolve(value(a));else p.reject(std::make_exception_ptr(std::runtime_error(id+" failed")));
   trace.push_back("settled:"+id);
  }else if(op=="flush"){loop.runReady();trace.push_back("flushed");}
  else if(op=="inspect"){
   Json result=Json::object();for(const auto& name:names){const auto v=stash.stashed(name);if(v)result[name]=*v?Json(**v):Json(nullptr);}
   trace.push_back({{"cache",result}});
  }else if(op=="clear"){
   try{stash.clearSystemPoetrySections([&]{trace.push_back("clearBeta");if(input.value("clearThrows",false))throw std::runtime_error("clear failed");});}
   catch(const std::exception& e){trace.push_back({{"clearError",e.what()}});}
  }
 }
 loop.runReady();return trace;
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}}
