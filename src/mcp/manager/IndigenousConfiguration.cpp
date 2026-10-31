#include "mcp/manager/IndigenousRuntime.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace batbox::mcp::manager {
namespace {
std::string expand(const std::string& value){
  std::string result;std::vector<std::string> missing;
  std::size_t at=0;
  while(at<value.size()){
    const auto begin=value.find("${",at);
    if(begin==std::string::npos){result+=value.substr(at);break;}
    result+=value.substr(at,begin-at);
    const auto end=value.find('}',begin+2);
    if(end==std::string::npos){result+=value.substr(begin);break;}
    const auto content=value.substr(begin+2,end-begin-2);
    if(content.empty()){result+="${}";at=end+1;continue;}
    const auto split=content.find(":-");const auto name=content.substr(0,split);
    const char* environment=std::getenv(name.c_str());
    if(environment)result+=environment;
    else if(split!=std::string::npos){
      // Keep the first two fields separated by ':-'; ignore any later fields.
      const auto next=content.find(":-",split+2);
      result+=content.substr(split+2,next==std::string::npos?next:next-split-2);
    }else{result+=value.substr(begin,end-begin+1);missing.push_back(name);}
    at=end+1;
  }
  if(!missing.empty()){
    std::string message="Missing MCP environment variables: ";
    for(std::size_t index=0;index<missing.size();++index){if(index)message+=", ";message+=missing[index];}
    throw std::runtime_error(message);
  }
  return result;
}
Json read(const std::filesystem::path& path){
  std::ifstream input(path);if(!input)return Json::object();
  auto value=Json::parse(input);
  if(!value.is_object())throw std::runtime_error("MCP configuration must be an object: "+runtime::pathToUtf8(path));
  return value;
}
void merge(Json& target,const Json& document){
  const auto& servers=document.contains("mcpServers")?document.at("mcpServers"):document;
  if(!servers.is_object())throw std::runtime_error("mcpServers must be an object");
  for(auto it=servers.begin();it!=servers.end();++it)target[it.key()]=it.value();
}
}
Json expandServerConfiguration(const Json& config){
  if(config.is_string())return expand(config.get<std::string>());
  if(config.is_array()){auto result=Json::array();for(const auto& item:config)result.push_back(expandServerConfiguration(item));return result;}
  if(config.is_object()){
    auto result=Json::object();
    for(auto it=config.begin();it!=config.end();++it)result[it.key()]=expandServerConfiguration(it.value());
    return result;
  }
  return config;
}
Json loadMcpConfigurations(const std::filesystem::path& privatePath,const std::filesystem::path& projectRoot){
  Json result=Json::object();merge(result,read(privatePath));
  merge(result,read(projectRoot/".mcp.json"));return result;
}
void IndigenousRuntime::loadConfiguration(){
  configurations_=inputs_.explicitConfigurations?inputs_.configurations:
    loadMcpConfigurations(inputs_.configHome/"mcp.json",inputs_.projectRoot);
  const auto state=read(inputs_.configHome/"mcp-state.json");
  disabled_=state.value("disabledMcpServers",Json::array());
  if(!disabled_.is_array())disabled_=Json::array();
}
void IndigenousRuntime::writeDisabled(){
  std::filesystem::create_directories(inputs_.configHome);
  const auto path=inputs_.configHome/"mcp-state.json";
  const auto temporary=inputs_.configHome/"mcp-state.json.tmp";
  {std::ofstream output(temporary,std::ios::trunc);
    output<<Json{{"disabledMcpServers",disabled_}}.dump(2)<<'\n';
    if(!output)throw std::runtime_error("Cannot save MCP connection settings");}
  std::error_code error;std::filesystem::rename(temporary,path,error);
#if defined(_WIN32)
  if(error){std::filesystem::remove(path,error);error.clear();std::filesystem::rename(temporary,path,error);}
#endif
  if(error)throw std::system_error(error,"Save MCP connection settings");
}
event_loop::Deferred<event_loop::Unit> IndigenousRuntime::reload(std::filesystem::path projectRoot){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("MCP reload requires owner");
  inputs_.projectRoot=std::move(projectRoot);loadConfiguration();
  return manager_->configure(configurations_);
}
} // namespace batbox::mcp::manager
