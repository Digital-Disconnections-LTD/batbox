#include "application/Config.hpp"
#include "application/Host.hpp"
#include "application/Subcommands.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <exception>
#include <iostream>
#include <vector>

#ifndef BATBOX_VERSION
#define BATBOX_VERSION "unknown"
#endif
#ifndef BATBOX_COMMIT
#define BATBOX_COMMIT "unknown"
#endif
#ifndef BATBOX_BUILD_DATE
#define BATBOX_BUILD_DATE "unknown"
#endif

namespace {
int run(std::vector<std::string> arguments){
  // Answer before reading any configuration, so packagers can probe the binary.
  if(batbox::application::requestsVersion(arguments)){
    // The first line stays "batbox X.Y.Z" for scripts and the Homebrew test.
    std::cout<<"batbox "<<BATBOX_VERSION<<"\ncommit "<<BATBOX_COMMIT<<
      "\nbuilt "<<BATBOX_BUILD_DATE<<'\n';return 0;
  }
  try{
    if(const auto status=batbox::application::runSubcommand(arguments))return *status;
    auto config=batbox::application::loadConfig(arguments);
    if(batbox::application::needsProviderSetup(config)){
      if(config.print){
        std::cerr<<"Batbox: "<<batbox::application::providerSetupHint()<<'\n';return 1;
      }
      batbox::application::beginProviderSetup(config);
    }
    batbox::application::Host host(std::move(config));
    return host.run();
  }catch(const std::exception& error){
    std::cerr<<"Batbox: "<<error.what()<<'\n';return 1;
  }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
  try{
    std::vector<std::string> arguments;
    arguments.reserve(argc>1?static_cast<std::size_t>(argc-1):0);
    for(int index=1;index<argc;++index)
      arguments.push_back(batbox::runtime::wideToUtf8(argv[index]));
    return run(std::move(arguments));
  }catch(const std::exception& error){
    std::cerr<<"Batbox: "<<error.what()<<'\n';return 1;
  }
}
#else
int main(int argc,char** argv){
  std::vector<std::string> arguments;
  arguments.reserve(argc>1?static_cast<std::size_t>(argc-1):0);
  for(int index=1;index<argc;++index)arguments.emplace_back(argv[index]);
  return run(std::move(arguments));
}
#endif
