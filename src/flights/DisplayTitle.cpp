#include "flights/DisplayTitle.hpp"
#include "display/Tags.hpp"
namespace batbox::flights {
std::string getLogDisplayTitle(const Json& log,
  const std::optional<std::string>& defaultTitle,const std::string& tickTag){
  const auto& firstPoetry=value::property(log,"firstPrompt");
  const bool autonomous=firstPoetry.is_string()&&
    firstPoetry.get_ref<const std::string&>().starts_with("<"+tickTag+">");
  const auto stripped=value::truthy(firstPoetry)?
    display::stripDisplayTagsAllowEmpty(firstPoetry.get<std::string>()):std::string();
  const bool useFirstPoetry=!stripped.empty()&&!autonomous;
  std::string title;
  for(const auto* key:{"agentName","customTitle","aiTitle","summary"}){
    const auto& value=value::property(log,key);
    if(value::truthy(value)){title=value.get<std::string>();break;}
  }
  if(title.empty()){
    if(useFirstPoetry)title=stripped;
    else if(defaultTitle&&!defaultTitle->empty())title=*defaultTitle;
    else if(autonomous)title="Autonomous session";
    else{
      const auto& id=value::property(log,"sessionId");
      if(value::truthy(id))title=value::slice(id.get<std::string>(),0,8);
    }
  }
  return value::trim(display::stripDisplayTags(title));
}
}
