#include "settings/schema/Internal.hpp"
#include <regex>

namespace batbox::settings::schema {
namespace {
SchemaResult entry(const Json& input){
  Json issues=Json::array();
  if(!input.is_object()){
    detail::invalidType(issues,Json::array(),"object",input);
    return {false,nullptr,issues};
  }
  Json result=Json::object();
  bool abort=false;
  int defined=0;
  if(input.contains("serverName")){
    const auto& name=input["serverName"];
    if(!name.is_string()){
      detail::invalidType(issues,Json::array({"serverName"}),"string",name);
      abort=true;
    }else{
      static const std::regex pattern(R"(^[a-zA-Z0-9_-]+$)");
      const auto text=name.get<std::string>();
      if(!std::regex_match(text,pattern))
        issues.push_back({{"origin","string"},{"code","invalid_format"},
          {"format","regex"},{"pattern","/^[a-zA-Z0-9_-]+$/"},
          {"path",Json::array({"serverName"})},
          {"message","Server name can only contain letters, numbers, "
            "hyphens, and underscores"}});
      else{result["serverName"]=name;++defined;}
    }
  }
  if(input.contains("serverCommand")){
    const auto& command=input["serverCommand"];
    if(!command.is_array()){
      detail::invalidType(issues,Json::array({"serverCommand"}),
        "array",command);abort=true;
    }else{
      Json parsed=Json::array();
      bool elementsValid=true;
      for(std::size_t i=0;i<command.size();++i){
        if(command[i].is_string())parsed.push_back(command[i]);
        else{
          detail::invalidType(issues,Json::array({"serverCommand",i}),
            "string",command[i]);
          elementsValid=false;abort=true;
        }
      }
      if(command.empty())
        issues.push_back({{"origin","array"},{"code","too_small"},
          {"minimum",1},{"inclusive",true},
          {"path",Json::array({"serverCommand"})},
          {"message","Server command must have at least one element "
            "(the command)"}});
      else if(elementsValid){result["serverCommand"]=parsed;++defined;}
    }
  }
  if(input.contains("serverUrl")){
    const auto& url=input["serverUrl"];
    if(url.is_string()){result["serverUrl"]=url;++defined;}
    else{
      detail::invalidType(issues,Json::array({"serverUrl"}),"string",url);
      abort=true;
    }
  }
  if(!abort&&defined!=1)
    issues.push_back({{"code","custom"},{"path",Json::array()},
      {"message","Entry must have exactly one of \"serverName\", "
        "\"serverCommand\", or \"serverUrl\""}});
  return {issues.empty(),issues.empty()?result:Json(nullptr),issues};
}
}
SchemaResult allowedMcpServerEntry(const Json& input){return entry(input);}
SchemaResult deniedMcpServerEntry(const Json& input){return entry(input);}
}
