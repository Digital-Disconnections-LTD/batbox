#include "application/Config.hpp"
#include "application/Host.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <algorithm>
#include <exception>
#include <iostream>
#include <vector>

#ifndef BATBOX_VERSION
#define BATBOX_VERSION "unknown"
#endif

namespace {
int run(std::vector<std::string> arguments){
  // Answer before reading any configuration, so packagers can probe the binary.
  const auto end=std::find(arguments.begin(),arguments.end(),"--");
  if(std::find(arguments.begin(),end,"--version")!=end){
    std::cout<<"batbox "<<BATBOX_VERSION<<'\n';return 0;
  }
  try{
    auto config=batbox::application::loadConfig(arguments);
    if(config.help){std::cout<<batbox::application::commandLineHelp();return 0;}
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
