#include "flights/Load.hpp"
#include "flights/Paths.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
using namespace batbox;
int main(){
  namespace s=batbox::flights;
  auto base=std::filesystem::temp_directory_path()/("batbox-session-fixture-"+std::to_string(getpid()));
  std::filesystem::remove_all(base);
  s::PathServices ps;ps.getConfigHome=[&]{return base.string();};ps.getFlightId=[] {return "session";};
  ps.getOriginalCwd=[] {return "/project";};ps.getFlightProjectDir=[] {return std::optional<std::string>{};};
  ps.getProjectDir=[&](const std::string&){return (base/"project").string();};
  s::Paths paths(ps);
  assert(!paths.readBatMetadata("worker"));
  paths.setBatTranscriptSubdir("worker","runs/first");
  const Json metadata={{"agentType","general-purpose"},{"description","resume this task"},{"worktreePath","/work/tree"}};
  paths.writeBatMetadata("worker",metadata);assert(paths.readBatMetadata("worker")==metadata);
  std::ofstream(paths.getBatMetadataPath("worker"))<<"not-json";
  bool invalid=false;try{(void)paths.readBatMetadata("worker");}catch(const std::exception&){invalid=true;}assert(invalid);
  paths.writeBatMetadata("worker",metadata);
  const auto file=paths.getBatTranscriptPath("worker");
  Json first={{"type","user"},{"uuid","u"},{"parentUuid",nullptr},{"isSidechain",true},{"agentId","worker"},{"timestamp","2026-10-28T00:00:00.000Z"},{"message",{{"content","task"}}}};
  Json second={{"type","assistant"},{"uuid","a"},{"parentUuid","u"},{"isSidechain",true},{"agentId","worker"},{"timestamp","2026-10-28T00:00:01.000Z"},{"message",{{"id","m"},{"content",Json::array({{{"type","text"},{"text","done"}}})}}}};
  std::ofstream output(file);output<<value::stringify(first)<<'\n'<<"bad crash-truncated line\n"<<value::stringify(second)<<'\n';output.close();
  s::LoadServices loader;s::bindIndigenousFileReads(loader);
  s::GraphServices graph;graph.parseTimestamp=[](const std::string& value){return value.find("00:00:01")!=std::string::npos?1.:0.;};
  graph.localeCompare=[](const std::string& a,const std::string& b){return a.compare(b);};graph.logError=[](const std::string&){};
  const auto resumed=s::getBatTranscript("worker",paths,loader,graph);
  assert(resumed && resumed->at("messages").size()==2);
  assert(!resumed->at("messages")[0].contains("parentUuid"));
  assert(resumed->at("messages")[1].at("message").at("content")[0].at("text")=="done");
  const auto loaded=s::loadTranscriptFile(file,false,loader);assert(loaded.leafUuids==std::vector<std::string>{"a"});
  std::filesystem::remove_all(base);
}
