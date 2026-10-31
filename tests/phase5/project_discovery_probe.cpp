#include "flights/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using namespace batbox;
Json evaluate(const Json& input){
  event_loop::Loop loop;Json calls=Json::array(),result; flights::ProjectDiscoveryServices services;
  services.realpath=[&](const std::string& path){
    calls.push_back({"realpath",path});event_loop::Deferred<std::string> result(loop);
    if(input.value("error",false))result.reject(std::make_exception_ptr(std::runtime_error("missing")));
    else result.resolve(input.at("resolved"));return result;
  };
  services.readdir=[&](const std::string& path,bool types){
    calls.push_back({"readdir",path,types});event_loop::Deferred<std::vector<flights::DirectoryEntry>> result(loop);
    if(types?input.value("fallbackError",false):!input.value("exact",false))result.reject(std::make_exception_ptr(std::runtime_error("missing")));
    else {
      std::vector<flights::DirectoryEntry> entries;
      if(types)for(const auto& entry:input.value("entries",Json::array()))entries.push_back({entry.at("name"),entry.at("directory")});result.resolve(std::move(entries));
    }
    return result;
  };
  if(input.value("native",false)){
    auto underlying=flights::indigenousProjectDiscoveryServices(loop);
    services.realpath=[&,real=underlying.realpath](const auto& path){calls.push_back({"realpath",path});return real(path);};
    services.readdir=[&,read=underlying.readdir](const auto& path,bool types){calls.push_back({"readdir",path,types});return read(path,types);};
  }
  auto dirs=std::make_shared<flights::ProjectDirectories>([&]{calls.push_back({"config"});return input.value("configHome","/config");});
  Json before;bool done=false;std::exception_ptr error;
  if(input.at("op")=="canonical"){
    auto pending=flights::canonicalizePath(loop,services,input.at("path"));before=calls;
    pending.then([&](const std::string* value,const auto& failure){error=failure;if(value)result={{"value",*value},{"defined",true}};done=true;});
  }else {
    auto pending=flights::findProjectDir(loop,services,dirs,input.at("path"));before=calls;
    pending.then([&](const std::optional<std::string>* value,const auto& failure){error=failure;if(value)result={{"value",*value?Json(**value):Json()},{"defined",value->has_value()}};done=true;});
  }
  loop.run();if(error)std::rethrow_exception(error);if(!done)throw std::runtime_error("Discovery promise did not settle");
  result["before"]=before;result["calls"]=calls;return result;
}
int main(){std::string line;while(std::getline(std::cin,line))std::cout<<value::stringify(evaluate(value::parse(line)))<<'\n';}
