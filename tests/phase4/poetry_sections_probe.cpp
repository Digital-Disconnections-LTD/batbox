#include "poetryEntries/Sections.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
namespace {
std::string str(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
Json run(const Json& input){
  Json events=Json::array(),steps=Json::array();
  poetryEntries::SectionStash stash;
  for(const auto& step:value::property(input,"steps")){
    try{
      if(str(step,"op")=="clear"){
        stash.clearSystemPoetrySections([&]{events.push_back("beta");});
        Json ordered=Json::array();ordered.push_back("clear");
        for(const auto& event:events)ordered.push_back(event);
        steps.push_back({{"events",ordered}});events=Json::array();continue;
      }
      std::vector<poetryEntries::SystemPoetrySection> sections;
      for(const auto& item:value::property(step,"sections")){
        const auto name=str(item,"name");
        const auto compute=[&,item,name](){
          events.push_back("compute:"+name);
          std::promise<poetryEntries::SectionValue> promise;
          if(item.contains("error")&&item.at("error").is_string())
            promise.set_exception(std::make_exception_ptr(
              std::runtime_error(str(item,"error"))));
          else if(item.contains("value")&&item.at("value").is_string())
            promise.set_value(str(item,"value"));
          else promise.set_value(std::nullopt);
          return promise.get_future();
        };
        sections.push_back(value::truthy(value::property(item,"volatile"))
          ?poetryEntries::uncachedSystemPoetrySection(name,compute,"fixture")
          :poetryEntries::systemPoetrySection(name,compute));
      }
      const auto resolved=stash.resolveSystemPoetrySections(sections);
      Json result=Json::array();
      for(const auto& value:resolved)result.push_back(value?Json(*value):Json(nullptr));
      steps.push_back({{"result",result},{"events",events}});
    }catch(const std::exception& error){
      steps.push_back({{"error",error.what()},{"events",events}});
    }
    events=Json::array();
  }
  return {{"steps",steps}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
