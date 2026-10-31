#include "application_connectors/Bootstrap.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdlib>
#include <fstream>
#include <future>
#include <map>
#include <stdexcept>

namespace batbox::application_connectors {
namespace {
std::filesystem::path home(){
  const char* value=std::getenv("HOME");
  if(!value||!*value)value=std::getenv("USERPROFILE");
  if(!value||!*value)throw std::runtime_error("Home directory is unavailable");
  return value;
}
std::string trim(std::string value){return value::trim(value);}
std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value.at(name).is_string()
    ?value.at(name).get<std::string>():"";
}
std::string privateSearxngEndpoint(const std::filesystem::path& path){
  std::ifstream input(path);
  std::string line,bindAddress;
  int port=0;
  while(std::getline(input,line)){
    line=trim(std::move(line));
    if(line.starts_with("bind_address:")){
      bindAddress=trim(line.substr(sizeof("bind_address:")-1));
      if(bindAddress.size()>=2&&
         ((bindAddress.front()=='\"'&&bindAddress.back()=='\"')||
          (bindAddress.front()=='\''&&bindAddress.back()=='\'')))
        bindAddress=bindAddress.substr(1,bindAddress.size()-2);
    }else if(line.starts_with("port:")){
      const auto selected=trim(line.substr(sizeof("port:")-1));
      try{std::size_t used=0;const auto value=std::stoi(selected,&used);
        if(used==selected.size()&&value>0&&value<=65535)port=value;
      }catch(...){}
    }
  }
  if(port&&(
      bindAddress=="127.0.0.1"||bindAddress=="localhost"||bindAddress=="::1"))
    return "http://127.0.0.1:"+std::to_string(port)+"/search";
  return "http://127.0.0.1:8888/search";
}
Json privateMcpServers(const std::filesystem::path& path){
  std::ifstream input(path);
  if(!input)return Json::object();
  const auto document=Json::parse(input);
  if(!document.is_object())throw std::invalid_argument("Private MCP config must be an object");
  const auto servers=document.value("mcpServers",document);
  if(!servers.is_object())throw std::invalid_argument("Private MCP servers must be an object");
  return servers;
}
std::vector<std::string> commandFor(const Json& entry){
  const auto command=field(entry,"command");
  if(command.empty())throw std::invalid_argument("MCP command required");
  std::vector<std::string> argv{command};
  if(entry.contains("args")){
    if(!entry.at("args").is_array())throw std::invalid_argument("MCP args must be an array");
    for(const auto& arg:entry.at("args")){
      if(!arg.is_string())throw std::invalid_argument("MCP args must be strings");
      argv.push_back(arg.get<std::string>());
    }
  }
  return argv;
}
std::map<std::string,std::string> environmentFor(const Json& entry){
  std::map<std::string,std::string> environment;
  if(!entry.contains("env"))return environment;
  if(!entry.at("env").is_object())throw std::invalid_argument("MCP env must be an object");
  for(auto it=entry.at("env").begin();it!=entry.at("env").end();++it){
    if(!it.value().is_string())throw std::invalid_argument("MCP env values must be strings");
    environment[it.key()]=it.value().get<std::string>();
  }
  return environment;
}
}

std::vector<std::string> scraplingServerArgv(const application::Config& config){
  if(config.scraplingCommand.empty())throw std::invalid_argument("Scrapling executable required");
  // Config stores the executable, not a shell command. Keep arguments as
  // distinct process argv so no shell interprets a user path.
  return {config.scraplingCommand,"mcp"};
}
void Bundle::shutdown()noexcept{
  for(auto& connection:mcpConnections)if(connection)connection->connected=false;
  for(auto& client:stdioClients)if(client)client->shutdown();
  if(scrapling)scrapling->shutdown();
  mcpConnections.clear();stdioClients.clear();scrapling.reset();searxng.reset();
}
std::shared_ptr<Bundle> bootstrap(event_loop::Loop& owner,
    const application::Config& config,DiscoveryPaths paths,
    std::function<void(const std::string&,const std::string&)> logDiagnostic,
    bool manageMcp){
  if(!owner.isOwnerThread())throw std::logic_error("Connector bootstrap requires owner thread");
  if(paths.privateMcpConfig.empty())paths.privateMcpConfig=
    application::configurationDirectory(config)/"mcp.json";
  if(paths.privateSearxngSettings.empty()){
    paths.privateSearxngSettings=application::configurationDirectory(config)/
      "searxng"/"settings.yml";
    if(!std::filesystem::exists(paths.privateSearxngSettings))
      paths.privateSearxngSettings=home()/".config"/"claude-private"/"searxng"/"settings.yml";
  }
  auto bundle=std::make_shared<Bundle>();
  auto diagnostic=[bundle,logDiagnostic](const std::string& server,
      const std::string& message){
    bundle->diagnostics.emplace_back(server,message);
    if(logDiagnostic)try{logDiagnostic(server,message);}catch(...){}
  };
  bundle->searxngEndpoint=config.searxngUrl.empty()?
    privateSearxngEndpoint(paths.privateSearxngSettings):config.searxngUrl;
  bundle->searxng=std::make_shared<Searxng>(bundle->searxngEndpoint);
  bundle->scrapling=std::make_shared<Scrapling>(scraplingServerArgv(config));
  if(manageMcp)return bundle;
  Json servers=config.mcpServers;
  if(!config.mcpServersConfigured)servers=privateMcpServers(paths.privateMcpConfig);
  std::vector<std::pair<std::string,std::future<std::shared_ptr<mcp::StdioClient>>>>
    starting;
  for(auto it=servers.begin();it!=servers.end();++it){
    const auto name=it.key();
    try{
      const auto& entry=it.value();
      if(!entry.is_object())throw std::invalid_argument("MCP server entry must be an object");
      const auto type=field(entry,"type");
      if(type=="http"||type=="sse"||(!field(entry,"url").empty())){
        bundle->pendingTransportServers.push_back(name);continue;
      }
      if(!type.empty()&&type!="stdio")throw std::invalid_argument("Unknown MCP transport");
      auto argv=commandFor(entry);
      auto environment=environmentFor(entry);
      starting.emplace_back(name,std::async(std::launch::async,
        [name,argv=std::move(argv),environment=std::move(environment)]{
          return std::make_shared<mcp::StdioClient>(argv,name,
            std::chrono::milliseconds(15000),environment);
        }));
    }catch(const std::exception& error){
      // A configured optional server cannot prevent the local terminal app
      // from starting. Never expose its argv or environment in diagnostics.
      bundle->unavailableServers.push_back(name);
      diagnostic(name,error.what());
    }catch(...){
      bundle->unavailableServers.push_back(name);
      diagnostic(name,"MCP server configuration failed");
    }
  }
  for(auto& [name,started]:starting){
    try{
      auto client=started.get();
      const auto diagnosticTail=client->stderrDiagnostic();
      if(!diagnosticTail.empty())diagnostic(name,"Server stderr: "+diagnosticTail);
      auto connection=mcp::makeStdioConnection(client,name,
        servers.at(name).value("skipPrefix",false));
      const auto dispatcher=owner.dispatcher();
      connection->postToOwner=[dispatcher](std::function<void()> action){
        dispatcher.postMission(std::move(action));};
      bundle->stdioClients.push_back(std::move(client));
      bundle->mcpConnections.push_back(std::move(connection));
    }catch(const std::exception& error){
      bundle->unavailableServers.push_back(name);
      diagnostic(name,error.what());
    }catch(...){
      bundle->unavailableServers.push_back(name);
      diagnostic(name,"MCP server startup failed");
    }
  }
  return bundle;
}
} // namespace batbox::application_connectors
