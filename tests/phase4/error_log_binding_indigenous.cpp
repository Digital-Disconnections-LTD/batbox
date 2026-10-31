#include "diagnostics/ErrorLogBindings.hpp"
#include "cleanup/Roster.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

int main(){
  using namespace batbox;
  namespace fs=std::filesystem;
  event_loop::Loop loop;
  cleanup::Roster cleanup(loop);
  const auto root=fs::temp_directory_path()/
    ("batbox-error-log-"+std::to_string(::getpid()));
  fs::create_directories(root/"mcp");
  ::setenv("BATBOX_INTERNAL_USER","1",1);
  auto stamp=[](){return "2026-03-31T12:34:56.000Z";};
  auto debug=std::make_shared<diagnostics::DebugLog>(
    diagnostics::indigenousDebugLogServices(loop,cleanup,stamp,
      []{return "session-1";},{"--debug-to-stderr"}));
  diagnostics::ErrorQueueServices imports;
  imports.hardFailFeature=[](){return false;};
  imports.argv=[](){return std::vector<std::string>{};};
  imports.env=[](const std::string&)->std::optional<std::string>{return std::nullopt;};
  imports.isEssentialTrafficOnly=[](){return false;};
  imports.timestampISO=stamp;
  imports.hardFailExit=[](const std::string&){throw std::runtime_error("hard fail");};
  diagnostics::ErrorLogQueue queue(imports);
  auto error=std::make_shared<diagnostics::ErrorValue>();
  error->message="before";error->stack="before stack";
  queue.logError(error);
  error->message="after";error->stack="after stack";
  auto sink=diagnostics::bindIndigenousErrorLogSink(loop,cleanup,debug,queue,
    [root]{return (root/"errors").string();},
    [root](const std::string& server){return (root/"mcp"/server).string();},
    stamp,[]{return "session-1";},[]{return "/work";},
    []{return "0.0.1";});
  queue.logMCPDebug("srv","connected");
  sink->flushLogWritersForTesting();
  const auto read=[](const fs::path& path){
    std::ifstream in(path);std::ostringstream text;text<<in.rdbuf();return text.str();};
  const auto errors=read(sink->getErrorsPath());
  const auto mcp=read(sink->getMCPLogsPath("srv"));
  assert(errors.find("after stack")!=std::string::npos);
  assert(errors.find("before stack")==std::string::npos);
  assert(mcp.find("connected")!=std::string::npos);
  assert(queue.getInMemoryErrors().at(0).at("error")=="before stack");
  fs::remove_all(root);
  std::cout<<"native queue, buffered sink and local filesystem binding pass\n";
}
