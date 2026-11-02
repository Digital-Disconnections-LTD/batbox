#include "application/Subcommands.hpp"
#include "application/Config.hpp"
#include "application/SetupWebScript.hpp"
#include "application_connectors/Status.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
  if(!ok)std::cout<<"\nSome checks need attention. Batbox still runs; the "
    "affected Gadgets report themselves unavailable.\n";
  return ok?0:1;
}

std::optional<int> runSubcommand(const std::vector<std::string>& arguments){
  if(arguments.empty())return std::nullopt;
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
  return std::nullopt;
}

std::string subcommandHelp(){
  return "\nCommands:\n"
    "  batbox setup web [--dry-run|--uninstall|--port N]\n"
    "                                 Install Scrapling and a local SearXNG service (no Docker)\n"
    "  batbox doctor | --doctor       Check web helpers and MCP servers; exit 1 if any fail\n";
}
} // namespace batbox::application
