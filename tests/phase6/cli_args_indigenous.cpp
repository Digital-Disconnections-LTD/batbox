#include "application/Config.hpp"
#include "application/Subcommands.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <cstdlib>
#endif

using namespace batbox;
namespace {
struct Temporary {
  std::filesystem::path path;
  Temporary(){
    path=std::filesystem::temp_directory_path()/
      ("batbox-cli-args-"+
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
  }
  ~Temporary(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
struct Outcome {
  std::optional<int> status;
  std::string out,err;
};
Outcome subcommand(const std::vector<std::string>& arguments){
  std::ostringstream out,err;
  auto* previousOut=std::cout.rdbuf(out.rdbuf());
  auto* previousErr=std::cerr.rdbuf(err.rdbuf());
  Outcome outcome;
  try{outcome.status=application::runSubcommand(arguments);}
  catch(...){std::cout.rdbuf(previousOut);std::cerr.rdbuf(previousErr);throw;}
  std::cout.rdbuf(previousOut);std::cerr.rdbuf(previousErr);
  outcome.out=out.str();outcome.err=err.str();
  return outcome;
}
std::string failure(const std::function<void()>& action){
  try{action();}catch(const std::invalid_argument& error){return error.what();}
  return {};
}
bool contains(const std::string& text,const std::string& part){
  return text.find(part)!=std::string::npos;
}
void isolate(const std::filesystem::path& root){
#ifdef _WIN32
  _putenv_s("BATBOX_CONFIG_DIR",runtime::pathToUtf8(root).c_str());
  for(const auto* name:{"BATBOX_PROFILE","BATBOX_MODEL","BATBOX_BASE_URL",
      "BATBOX_AUTH_TOKEN","BATBOX_API_KEY_HELPER"})_putenv_s(name,"");
#else
  setenv("BATBOX_CONFIG_DIR",root.c_str(),1);
  for(const auto* name:{"BATBOX_PROFILE","BATBOX_MODEL","BATBOX_BASE_URL",
      "BATBOX_AUTH_TOKEN","BATBOX_API_KEY_HELPER"})unsetenv(name);
#endif
}
void versionRequests(){
  using Arguments=std::vector<std::string>;
  assert(application::requestsVersion(Arguments{"version"}));
  assert(application::requestsVersion(Arguments{"--version"}));
  assert(application::requestsVersion(Arguments{"--cwd","project","--version"}));
  assert(!application::requestsVersion(Arguments{}));
  assert(!application::requestsVersion(Arguments{"-p","version"}));
  assert(!application::requestsVersion(Arguments{"-p","--","--version"}));
  assert(!application::requestsVersion(Arguments{"versions"}));
}
void unknownCommands(){
  for(const auto& arguments:std::vector<std::vector<std::string>>{
      {"update"},{"upgrade","now"},{"Fix the failing test"},{"hello","-p"},{""}}){
    const auto outcome=subcommand(arguments);
    assert(outcome.status==2);
    assert(outcome.out.empty());
    assert(outcome.err.starts_with("batbox: unknown command \""+arguments.front()+"\"\n"));
    assert(contains(outcome.err,application::commandLineUsage()));
    assert(contains(outcome.err,"batbox -p \"your prompt\""));
    assert(contains(outcome.err,"--prompt"));
    assert(contains(outcome.err,"batbox help"));
  }
}
void helpRequests(){
  const auto help=application::commandLineHelp()+application::subcommandHelp();
  for(const auto& arguments:std::vector<std::vector<std::string>>{
      {"help"},{"--help"},{"-h"},{"--inline","--help"},{"-p","-h"},{"help","providers"}}){
    const auto outcome=subcommand(arguments);
    assert(outcome.status==0);
    assert(outcome.out==help);
    assert(outcome.err.empty());
  }
  assert(contains(help,application::commandLineUsage()));
  for(const auto* part:{"--prompt TEXT","-p, --print [TEXT]","batbox help","batbox version",
      "setup web, doctor, providers, help, version"})
    assert(contains(help,part));
  assert(!contains(help,"[poetry]"));
}
void launchesBatbox(){
  for(const auto& arguments:std::vector<std::vector<std::string>>{
      {},{"--inline"},{"-p","hello"},{"--print","--","help"},{"--prompt","update"},
      {"--resume","slug"},{"-c"}}){
    const auto outcome=subcommand(arguments);
    assert(!outcome.status);
    assert(outcome.out.empty()&&outcome.err.empty());
  }
}
void prompts(const std::filesystem::path& root){
  const auto client=root/"native-client.json";
  {std::ofstream file(client);
    file<<R"({"model":"fixture","base_url":"http://127.0.0.1:9","mcpServers":{}})";}
  const auto load=[&](std::vector<std::string> flags){
    flags.insert(flags.begin(),{"--config",runtime::pathToUtf8(client)});
    return application::loadConfig(flags);
  };
  const auto plain=load({});
  assert(!plain.print&&!plain.initialPoetry);
  const auto printed=load({"-p","hello","world"});
  assert(printed.print&&printed.initialPoetry=="hello world");
  assert(load({"--print","--","--help"}).initialPoetry=="--help");
  assert(!load({"--print"}).initialPoetry);
  const auto prompted=load({"--prompt","Fix the failing test"});
  assert(!prompted.print&&prompted.initialPoetry=="Fix the failing test");
  assert(load({"--inline","--prompt","version"}).initialPoetry=="version");
  assert(load({"-p","--prompt","Summarize"}).initialPoetry=="Summarize");
  const auto resumed=load({"--resume","parser","--prompt","next step"});
  assert(resumed.resumeFlight=="parser"&&resumed.initialPoetry=="next step");
  assert(load({"-p","--resume","parser","next step"}).initialPoetry=="next step");
  const auto stray=failure([&]{(void)load({"--inline","hello"});});
  assert(contains(stray,"Unexpected argument \"hello\"")&&contains(stray,"--prompt \"text\"")&&
    contains(stray,"-p \"text\""));
  assert(contains(failure([&]{(void)load({"--","hello"});}),"Unexpected argument \"hello\""));
  assert(contains(failure([&]{(void)load({"--resume","parser","next step"});}),
    "Unexpected argument \"next step\""));
  assert(contains(failure([&]{(void)load({"--prompt"});}),"--prompt requires a value"));
  assert(contains(failure([&]{(void)load({"--prompt",""});}),"--prompt needs text"));
  assert(contains(failure([&]{(void)load({"--prompt"," \n"});}),"--prompt needs text"));
  assert(contains(failure([&]{(void)load({"--prompt","a","--prompt","b"});}),
    "--prompt can be given once"));
  assert(contains(failure([&]{(void)load({"-p","--prompt","a","b"});}),"Give the prompt once"));
  assert(contains(failure([&]{(void)load({"-r","--prompt","a"});}),
    "--resume <slug> --prompt"));
}
} // namespace

int main(){
  Temporary root;
  isolate(root.path);
  versionRequests();
  unknownCommands();
  helpRequests();
  launchesBatbox();
  prompts(root.path);
  return 0;
}
