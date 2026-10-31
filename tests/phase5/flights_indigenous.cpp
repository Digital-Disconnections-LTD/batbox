#include "flights/Load.hpp"
#include "flights/Project.hpp"
#include "runtime/MessageSnapshot.hpp"
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
  {
    std::unordered_map<std::string,runtime::OrderedStringSet> recorded;
    s::ProjectServices services;services.paths=&paths;
    services.isTestEnvironment=[] {return false;};services.allowTestPersistence=[] {return false;};
    services.isPersistenceDisabled=[] {return false;};services.skipPoetryHistory=[] {return false;};
    services.settings=[] {return Json::object();};services.getBranch=[] {return std::optional<std::string>("main");};
    services.getPoetryId=[] {return std::optional<std::string>{};};services.getFlightId=[] {return "session";};
    services.getUserType=[] {return "external";};services.getEntrypoint=[] {return "cli";};services.getCwd=[] {return "/project";};services.getVersion=[] {return "test-version";};
    services.getPlanSlug=[](const std::string&){return std::optional<std::string>{};};
    services.getFlightMessages=[&](const std::string& id)->runtime::OrderedStringSet&{return recorded[id];};
    services.statFile=[](const std::string& path){(void)std::filesystem::file_size(path);};services.logError=[](const std::string&){};
    services.nowIso=[] {return "2026-10-28T00:00:00.000Z";};
    services.poetryEntries.extractTag=[](const std::string&,const std::string&){return std::optional<std::string>{};};
    services.poetryEntries.builtInCommandNames=[] {return std::set<std::string>{};};
    auto queue=std::make_shared<s::WriteQueue>(s::indigenousQueueServices(),std::chrono::seconds(10));
    s::Project project(services,queue);project.currentFlightMode="coordinator";
    const Json user={{"type","user"},{"uuid","new-u"},{"timestamp","2026-10-28T00:00:00.000Z"},{"message",{{"content","task"}}}};
    const Json assistant={{"type","assistant"},{"uuid","new-a"},{"timestamp","2026-10-28T00:00:01.000Z"},{"message",{{"id","model"},{"content",Json::array({{{"type","text"},{"text","answer"}}})}}}};
    const Json next={{"type","user"},{"uuid","new-v"},{"timestamp","2026-10-28T00:00:02.000Z"},{"message",{{"content","next"}}}};
    project.insertMessageChain(Json::array({user,assistant}));
    project.insertMessageChain(Json::array({user,assistant,next}),true,"resume");
    assert(recorded["session"].size()==2);
    project.insertMessageChain(Json::array({next}),false,std::nullopt,"new-a");project.flush();
    const auto main=s::loadTranscriptFile(paths.getTranscriptPath(),false,loader);
    assert(main.messages.values().size()==3 && main.modes.at("session")=="coordinator");
    assert(main.messages.get("new-v")->at("parentUuid")=="new-a");
    const auto child=s::getBatTranscript("resume",paths,loader,graph);
    assert(child && child->at("messages").size()==3);
    auto live=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","live-a"},{"timestamp","2026-10-28T00:00:03.000Z"},{"message",{{"id","live-model"},{"usage",{{"output_tokens",0}}},{"content",Json::array({{{"type","text"},{"text","live answer"}}})}}}});
    auto virtualMessage=std::make_shared<Json>(*live);(*virtualMessage)["uuid"]="virtual-a";(*virtualMessage)["isVirtual"]=true;
    auto records=project.recordServices({});const std::vector<s::MessageRef> references={live,virtualMessage};
    assert(s::recordTranscriptRefs(references,Json(),"new-v",references,records)=="virtual-a");
    {runtime::MessageLock lock(live);lock.value()["message"]["usage"]["output_tokens"]=100;lock.value()["message"]["stop_reason"]="end_turn";lock.value()["cwd"]="/later";}
    {runtime::MessageLock lock(virtualMessage);lock.value()["message"]["usage"]["output_tokens"]=200;}
    project.flush();const auto persisted=s::loadTranscriptFile(paths.getTranscriptPath(),false,loader);
    assert(persisted.messages.get("live-a")->at("message").at("usage").at("output_tokens")==100);
    assert(persisted.messages.get("live-a")->at("cwd")=="/project");
    assert(persisted.messages.get("virtual-a")->at("message").at("usage").at("output_tokens")==0);
    assert(!persisted.messages.get("virtual-a")->contains("isVirtual"));
  }
  std::filesystem::remove_all(base);
}
