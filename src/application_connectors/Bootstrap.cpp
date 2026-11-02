#include "application_connectors/Bootstrap.hpp"
#include "application_connectors/Status.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <future>
#include <map>
#include <stdexcept>

namespace batbox::application_connectors {
namespace {
std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value.at(name).is_string()
    ?value.at(name).get<std::string>():"";
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
  if(paths.privateSearxngSettings.empty())
    paths.privateSearxngSettings=searxngSettingsPath(config);
  auto bundle=std::make_shared<Bundle>();
  auto diagnostic=[bundle,logDiagnostic](const std::string& server,
      const std::string& message){
    bundle->diagnostics.emplace_back(server,message);
    if(logDiagnostic)try{logDiagnostic(server,message);}catch(...){}
  };
  bundle->searxngEndpoint=searxngTarget(config,paths.privateSearxngSettings).url;
  bundle->searxng=std::make_shared<Searxng>(bundle->searxngEndpoint);
  // Scrapling starts on the first web Gadget call, so a missing helper costs
  // nothing here.
  bundle->scrapling=std::make_shared<Scrapling>(scraplingServerArgv(config),
    Scrapling::defaultTimeout,[command=config.scraplingCommand]{
      return installedScraplingVersion(command);});
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
