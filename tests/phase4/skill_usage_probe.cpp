#include "skills/usage/SkillUsage.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace usage=batbox::skills::usage;
namespace value=batbox::value;
namespace {
std::string name(const Json& item){
  const auto& val=value::property(item,"name");
  return val.is_string()?val.get<std::string>():std::string{};
}
Json run(const Json& input){
  usage::State state;usage::Services services;
  Json config=value::property(input,"config");
  if(config.is_null())config=Json::object();
  std::int64_t now=0;int saves=0;
  services.nowMillis=[&]{return now;};
  services.getGlobalConfig=[&]{return config;};
  services.saveGlobalConfig=[&](const std::function<Json(const Json&)>& update){
    config=update(config);++saves;
  };
  Json outputs=Json::array();
  for(const auto& action:input.at("actions")){
    now=value::property(action,"at").get<std::int64_t>();
    try{
      Json value=nullptr;
      if(value::property(action,"op")=="record")
        usage::recordSkillUsage(name(action),state,services);
      else value=usage::getSkillUsageScore(name(action),services);
      outputs.push_back(Json{{"value",value},{"config",config},{"saves",saves}});
    }catch(const std::exception& error){
      outputs.push_back(Json{{"error",error.what()},{"config",config},{"saves",saves}});
    }
  }
  return outputs;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
  }
}
