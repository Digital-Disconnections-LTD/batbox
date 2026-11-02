#include "application/Subcommands.hpp"
#include "application/Config.hpp"
#include "application/SetupWebScript.hpp"
#include "application_config/EndpointStore.hpp"
#include "application_connectors/Status.hpp"
#include "providers/LocalDiscovery.hpp"
#include "providers/Onboarding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace batbox::application {
namespace {
Config fallbackConfig(){
  Config config;
  const char* directory=std::getenv("BATBOX_CONFIG_DIR");
  const char* home=std::getenv("HOME");
  if(!home||!*home)home=std::getenv("USERPROFILE");
  if(directory&&*directory)config.configDirectory=std::filesystem::absolute(
    runtime::pathFromUtf8(directory));
  else if(home&&*home)config.configDirectory=runtime::pathFromUtf8(home)/".batbox";
  config.configPath=config.configDirectory/"native-client.json";
  if(const char* value=std::getenv("BATBOX_SCRAPLING_COMMAND");value&&*value)
    config.scraplingCommand=value;
  if(const char* value=std::getenv("BATBOX_SEARXNG_URL");value&&*value)
    config.searxngUrl=value;
  return config;
}
// Configuration file locations even when the current file cannot be loaded.
Config configLocation(const std::vector<std::string>& arguments){
  try{return loadConfig(arguments);}
  catch(...){
    auto config=fallbackConfig();
    for(std::size_t i=0;i+1<arguments.size();++i)
      if(arguments[i]=="--config")config.configPath=runtime::pathFromUtf8(arguments[i+1]);
    return config;
  }
}
std::filesystem::path legacyPath(const Config& config){
  return config.legacyConfigDirectory.empty()?std::filesystem::path{}:
    config.legacyConfigDirectory/"native-client.json";
}
std::map<std::string,EndpointRecord> existingEndpoints(const Config& config){
  auto path=config.configPath;
  if(!std::filesystem::exists(path)&&!legacyPath(config).empty()&&
      std::filesystem::exists(legacyPath(config)))path=legacyPath(config);
  if(!std::filesystem::exists(path))return {};
  auto loaded=application_config::readEndpoints(path);
  return loaded?*loaded:std::map<std::string,EndpointRecord>{};
}
std::string padded(const std::string& text,std::size_t width){
  return text.size()>=width?text+" ":text+std::string(width-text.size(),' ');
}
std::string joinedModels(const std::vector<std::string>& models,std::size_t limit=6){
  std::string text;
  for(std::size_t i=0;i<models.size()&&i<limit;++i)text+=(i?", ":"")+models[i];
  if(models.size()>limit)text+=", +"+std::to_string(models.size()-limit)+" more";
  return text;
}
void printLocalServers(std::ostream& out,const std::vector<providers::LocalServer>& servers){
  out<<"Model servers on this machine\n";
  if(servers.empty())out<<"  none found (checked Ollama :11434, LM Studio :1234, "
    "llama-server :9931 and :8080, vLLM :8000)\n";
  for(const auto& server:servers){
    out<<"  "<<padded(server.preset,14)<<providers::describeLocalServer(server)<<" · "
      <<server.baseUrl<<"\n";
    if(!server.models.empty())out<<"  "<<std::string(14,' ')<<joinedModels(server.models)<<"\n";
  }
}
int providersUsage(){
  std::cerr<<"Usage:\n"
    "  batbox providers                 List the catalog and local model servers\n"
    "  batbox providers show <id>       Show one provider and its models\n"
    "  batbox providers add <id> [--key-env VAR | --key -] [--base-url URL]\n"
    "                       [--model ID] [--id NAME] [--no-test]\n";
  return 2;
}
int listProviders(const Config& config){
  const auto& catalog=providers::bundledCatalog();
  const auto environment=providers::processEnvironment();
  printLocalServers(std::cout,providers::discoverLocalServers());
  std::cout<<"\nConfigured endpoints\n";
  const auto endpoints=existingEndpoints(config);
  if(endpoints.empty())std::cout<<"  none yet\n";
  for(const auto& [id,endpoint]:endpoints){
    std::cout<<"  "<<padded(id,14)<<endpoint.label<<" · "<<endpoint.models.size()
      <<(endpoint.models.size()==1?" model":" models")<<" · "<<endpoint.provider;
    if(config.selectedProfile.starts_with(id+"::"))std::cout<<" · active";
    std::cout<<"\n";
  }
  for(const std::string section:{"Cloud","Local servers","Unsupported yet"}){
    std::cout<<"\n"<<section<<"\n";
    for(const auto& provider:catalog.providers){
      if(providers::catalogSection(provider)!=section)continue;
      std::cout<<"  "<<padded(provider.id,26)<<padded(provider.name,30);
      if(!provider.supported()){std::cout<<provider.reason<<"\n";continue;}
      std::cout<<providers::kindLabel(provider.kind);
      if(provider.comingSoon())std::cout<<" · sign-in coming soon";
      const auto present=providers::presentKeyEnv(provider,environment);
      if(!present.empty())std::cout<<" · $"<<present.front()<<" set";
      std::cout<<"\n";
    }
  }
  std::cout<<"\n"<<catalog.providers.size()<<" providers from models.dev. Connect one with\n"
    "  batbox providers add <id> [--key-env VAR | --key -] [--base-url URL] [--model ID]\n"
    "or run /providers inside Batbox.\n";
  return 0;
}
int showProvider(const std::string& id){
  const auto* provider=providers::bundledCatalog().find(id);
  if(!provider){std::cerr<<"batbox providers: no provider \""<<id<<"\" in the catalog\n";return 2;}
  std::cout<<provider->name<<" ("<<provider->id<<")\n";
  if(!provider->supported()){std::cout<<"  Not supported yet: "<<provider->reason<<"\n";return 0;}
  std::cout<<"  Speaks     "<<providers::kindLabel(provider->kind)<<"\n"
    <<"  Base URL   "<<provider->baseUrl<<"\n"
    <<"  Path       "<<provider->messagesPath<<"\n";
  if(!provider->modelsPath.empty())std::cout<<"  Models     "<<provider->modelsPath<<"\n";
  std::cout<<"  Auth       "<<(provider->auth=="key"?provider->authHeader:provider->auth);
  if(!provider->keyEnv.empty())std::cout<<" · key from $"<<provider->keyEnv.front();
  std::cout<<"\n";
  if(!provider->doc.empty())std::cout<<"  Docs       "<<provider->doc<<"\n";
  if(!provider->note.empty())std::cout<<"  Note       "<<provider->note<<"\n";
  if(!provider->models.empty())std::cout<<"\nModels\n";
  for(const auto& model:provider->models)
    std::cout<<"  "<<padded(model.id,40)<<providers::describeModel(model)<<"\n";
  return 0;
}
int addProvider(const std::vector<std::string>& arguments){
  if(arguments.empty())return providersUsage();
  const auto& catalog=providers::bundledCatalog();
  const auto* provider=catalog.find(arguments.front());
  if(!provider){
    std::cerr<<"batbox providers add: no provider \""<<arguments.front()<<
      "\" in the catalog; run batbox providers to list them\n";
    return 2;
  }
  if(!provider->supported()){
    std::cerr<<"batbox providers add: "<<provider->name<<" is not supported yet. "
      <<provider->reason<<"\n";
    return 2;
  }
  if(provider->comingSoon()){
    std::cerr<<"batbox providers add: "<<provider->name<<" needs its device sign-in, "
      "which is coming soon.\n";
    return 2;
  }
  providers::Setup setup;
  std::string endpointId,configArgument;
  bool keyFromStdin=false,test=true;
  for(std::size_t i=1;i<arguments.size();++i){
    const auto& argument=arguments[i];
    const auto next=[&]()->std::string{
      if(i+1>=arguments.size())throw std::invalid_argument(argument+" needs a value");
      return arguments[++i];
    };
    if(argument=="--key-env")setup.keyEnv=next();
    else if(argument=="--key"){
      if(next()!="-")throw std::invalid_argument(
        "--key reads the key from stdin; pass --key - (keys on the command line leak into shell history)");
      keyFromStdin=true;
    }
    else if(argument=="--base-url")setup.baseUrl=next();
    else if(argument=="--model")setup.model=next();
    else if(argument=="--id")endpointId=next();
    else if(argument=="--config")configArgument=next();
    else if(argument=="--no-test")test=false;
    else throw std::invalid_argument("Unknown option for batbox providers add: "+argument);
  }
  if(keyFromStdin&&!setup.keyEnv.empty())
    throw std::invalid_argument("Use either --key - or --key-env, not both");
  if(keyFromStdin){
    std::getline(std::cin,setup.key);
    while(!setup.key.empty()&&(setup.key.back()=='\r'||setup.key.back()==' '))setup.key.pop_back();
    if(setup.key.empty())throw std::invalid_argument("No key arrived on stdin");
  }
  const auto environment=providers::processEnvironment();
  if(setup.key.empty()&&setup.keyEnv.empty()&&providers::takesKey(*provider)){
    const auto present=providers::presentKeyEnv(*provider,environment);
    if(!present.empty())setup.keyEnv=present.front();
    else if(!provider->keyEnv.empty()){
      setup.keyEnv=provider->keyEnv.front();
      std::cerr<<"Note: $"<<setup.keyEnv<<" is not set in this shell; Batbox reads it "
        "when it sends a request.\n";
    }
  }
  if(setup.baseUrl.empty())setup.baseUrl=providers::resolvedBaseUrl(*provider,environment);
  if(providers::needsBaseUrlEdit(setup.baseUrl))
    throw std::invalid_argument("Pass --base-url: the catalog URL has a placeholder ("+
      setup.baseUrl+")");
  std::vector<std::string> configArguments;
  if(!configArgument.empty())configArguments={"--config",configArgument};
  const auto config=configLocation(configArguments);
  auto endpoints=existingEndpoints(config);
  if(endpointId.empty())endpointId=providers::uniqueEndpointId(endpoints,provider->id);
  if(test){
    auto probe=setup;
    if(probe.model.empty())probe.model=provider->models.empty()?"probe":provider->models.front().id;
    const auto result=providers::testConnection(providers::buildEndpoint(*provider,probe,
      catalog.defaultContext,catalog.defaultOutput));
    std::cout<<(result.ok?"Connection: ":"Connection check: ")<<result.message<<"\n";
    if(result.ok)setup.liveModels=result.models;
  }
  if(setup.model.empty()){
    if(!setup.liveModels.empty())setup.model=setup.liveModels.front();
    else if(!provider->models.empty())setup.model=provider->models.front().id;
    else throw std::invalid_argument("Pass --model: the server listed no models");
  }
  const auto endpoint=providers::buildEndpoint(*provider,setup,catalog.defaultContext,
    catalog.defaultOutput);
  const auto saved=providers::saveEndpoint(config.configPath,legacyPath(config),
    std::move(endpoints),endpointId,endpoint,setup.model);
  std::cout<<providers::addedNotice(saved.endpointId,setup.model)<<"\n"
    <<"Saved to "<<runtime::pathToUtf8(config.configPath)<<" (private, mode 0600)\n";
  return 0;
}
int setupWeb(const std::vector<std::string>& arguments){
#ifdef _WIN32
  (void)arguments;
  std::cerr<<"batbox setup web needs bash on macOS or Linux. On Windows, install "
    "Scrapling with pipx and point searxng_url at a SearXNG instance; see docs/connectors.md.\n";
  return 2;
#else
  std::vector<std::string> argv{"bash","-c",setupWebScript,"batbox setup web"};
  argv.insert(argv.end(),arguments.begin(),arguments.end());
  std::vector<char*> raw;
  for(auto& argument:argv)raw.push_back(argument.data());
  raw.push_back(nullptr);
  ::execvp("bash",raw.data());
  std::cerr<<"batbox setup web: cannot run bash: "<<std::strerror(errno)<<'\n';
  return 127;
#endif
}
}

int runDoctor(const std::vector<std::string>& arguments){
  Config config;
  std::string configurationError;
  try{config=loadConfig(arguments);}
  catch(const std::exception& error){
    configurationError=error.what();
    config=fallbackConfig();
  }
  const auto web=application_connectors::checkWebHelpers(config);
  const auto mcp=application_connectors::checkMcpConfiguration(config);
  std::cout<<application_connectors::formatDoctor(web,mcp,configurationError);
  const bool ok=configurationError.empty()&&
    std::all_of(web.begin(),web.end(),[](const auto& check){return check.ok;})&&
    std::all_of(mcp.begin(),mcp.end(),[](const auto& check){return check.ok;});
  std::cout<<"\n";
  printLocalServers(std::cout,providers::discoverLocalServers());
  std::cout<<"\nModel endpoints\n";
  if(config.endpoints.empty()&&config.model.empty())
    std::cout<<"  none configured · run: batbox providers\n";
  for(const auto& [id,endpoint]:config.endpoints)
    std::cout<<"  "<<padded(id,14)<<endpoint.label<<" · "<<endpoint.provider<<" · "
      <<endpoint.models.size()<<(endpoint.models.size()==1?" model":" models")
      <<(config.selectedProfile.starts_with(id+"::")?" · active":"")<<"\n";
  if(config.endpoints.empty()&&!config.model.empty())
    std::cout<<"  "<<config.model<<" at "<<config.baseUrl<<"\n";
  if(!ok)std::cout<<"\nSome checks need attention. Batbox still runs; the "
    "affected Gadgets report themselves unavailable.\n";
  return ok?0:1;
}

int runProviders(const std::vector<std::string>& arguments){
  try{
    if(arguments.empty()||arguments.front()=="list"){
      std::vector<std::string> rest;
      if(!arguments.empty())rest.assign(arguments.begin()+1,arguments.end());
      return listProviders(configLocation(rest));
    }
    if(arguments.front()=="add")return addProvider({arguments.begin()+1,arguments.end()});
    if(arguments.front()=="show"&&arguments.size()==2)return showProvider(arguments[1]);
    if(arguments.front()=="--help"||arguments.front()=="-h"){providersUsage();return 0;}
    return providersUsage();
  }catch(const std::exception& error){
    std::cerr<<"batbox providers: "<<error.what()<<"\n";
    return 2;
  }
}

bool needsProviderSetup(const Config& config){
  return config.model.empty()&&config.profiles.empty()&&config.endpoints.empty();
}

std::string providerSetupHint(){
  return "No model server is configured. Run `batbox providers` to see local servers and "
    "the provider catalog, then `batbox providers add <id>` (or start Batbox without "
    "--print to pick one). --model and --base-url still work for a single server.";
}

void beginProviderSetup(Config& config){
  config.providerSetup=true;
  config.model=kProviderSetupProfile;
}

bool requestsVersion(const std::vector<std::string>& arguments){
  if(!arguments.empty()&&arguments.front()=="version")return true;
  const auto end=std::find(arguments.begin(),arguments.end(),"--");
  return std::find(arguments.begin(),end,"--version")!=end;
}

std::optional<int> runSubcommand(const std::vector<std::string>& arguments){
  if(arguments.empty())return std::nullopt;
  if(arguments.front()=="providers")return runProviders({arguments.begin()+1,arguments.end()});
  if(arguments.front()=="setup"){
    if(arguments.size()<2||arguments[1]!="web"){
      std::cerr<<"Usage: batbox setup web [--dry-run] [--uninstall] [--port N] [--help]\n";
      return 2;
    }
    return setupWeb({arguments.begin()+2,arguments.end()});
  }
  const auto doctor=std::find(arguments.begin(),arguments.end(),"--doctor");
  if(arguments.front()=="doctor"||doctor!=arguments.end()){
    std::vector<std::string> rest;
    for(auto it=arguments.begin();it!=arguments.end();++it){
      if(it==doctor||(it==arguments.begin()&&*it=="doctor"))continue;
      rest.push_back(*it);
    }
    return runDoctor(rest);
  }
  const auto end=std::find(arguments.begin(),arguments.end(),"--");
  if(arguments.front()=="help"||std::find(arguments.begin(),end,"--help")!=end||
      std::find(arguments.begin(),end,"-h")!=end){
    std::cout<<commandLineHelp()<<subcommandHelp();
    return 0;
  }
  if(!arguments.front().starts_with('-')){
    std::cerr<<"batbox: unknown command \""<<arguments.front()<<"\"\n"<<commandLineUsage()<<
      "To send a prompt, quote it after -p or --prompt: batbox -p \"your prompt\"\n"
      "Run batbox help for every option and command.\n";
    return 2;
  }
  return std::nullopt;
}

std::string subcommandHelp(){
  return "\nCommands:\n"
    "  batbox setup web [--dry-run|--uninstall|--port N]\n"
    "                                 Install Scrapling and a local SearXNG service (no Docker)\n"
    "  batbox doctor | --doctor       Check web helpers, MCP servers and model servers\n"
    "  batbox providers [show <id>]   List the provider catalog and local model servers\n"
    "  batbox providers add <id> [--key-env VAR | --key -] [--base-url URL] [--model ID]\n"
    "                                 Save a provider as an endpoint without the TUI\n";
}
} // namespace batbox::application
