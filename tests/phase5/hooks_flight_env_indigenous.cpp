#include <cstdlib>
#include "instructions/hooks/runtime/FlightEnvironment.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
void write(const std::string& path,const std::string& text){
  std::ofstream stream(path);stream<<text;
  if(!stream)throw std::runtime_error("Fixture file write failed");
}
std::string read(const std::string& path){
  std::ifstream stream(path);return std::string(std::istreambuf_iterator<char>(stream),{});
}
}
int main(){
  char pattern[]="/tmp/batbox-session-env-XXXXXX";
  const auto* made=::mkdtemp(pattern);
  if(!made)throw std::runtime_error("mkdtemp failed");
  const std::string root=made;
  try{
    event_loop::Loop loop;
    std::vector<std::string> logs;
    auto imports=hook::indigenousFlightEnvironmentImports(loop,
      [root]{return root;},[]{return std::string("session-one");},
      []{return std::string("linux");},
      []()->std::optional<std::string>{return std::nullopt;},
      [&](const std::string& text){logs.push_back(text);});
    hook::FlightEnvironment flight(loop,std::move(imports));
    std::string path;
    flight.getHookEnvFilePath("Setup",2).then(
      [&](const std::string* value,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);path=*value;});
    loop.run();
    const auto directory=root+"/session-env/session-one";
    assert(path==directory+"/setup-hook-2.sh");
    assert(std::filesystem::is_directory(directory));
    write(directory+"/filechanged-hook-1.sh","export C=3\n");
    write(directory+"/setup-hook-2.sh","export B=2\n");
    write(directory+"/setup-hook-1.sh","export A=1\n");
    write(directory+"/cwdchanged-hook-4.sh","export D=4\n");
    std::optional<std::string> loaded;
    flight.getFlightEnvironmentScript().then(
      [&](const std::optional<std::string>* value,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);loaded=*value;});
    loop.run();
    assert(loaded=="export A=1\nexport B=2\nexport D=4\nexport C=3");
    write(directory+"/setup-hook-1.sh","export A=changed\n");
    flight.getFlightEnvironmentScript().then(
      [&](const std::optional<std::string>* value,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);loaded=*value;});
    loop.run();
    assert(loaded->starts_with("export A=1\n"));
    flight.invalidateFlightEnvStash();
    flight.getFlightEnvironmentScript().then(
      [&](const std::optional<std::string>* value,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);loaded=*value;});
    loop.run();
    assert(loaded->starts_with("export A=changed\n"));
    flight.clearCwdEnvFiles().then(
      [&](const event_loop::Unit*,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);});
    loop.run();
    assert(read(directory+"/filechanged-hook-1.sh").empty());
    assert(read(directory+"/cwdchanged-hook-4.sh").empty());
    assert(read(directory+"/setup-hook-1.sh")=="export A=changed\n");
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
  std::cout<<"native session environment files, priority, cache and clear pass\n";
}
