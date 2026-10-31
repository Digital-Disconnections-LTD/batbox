#include <cstdlib>
#include "flights/AsyncStash.hpp"
#include "flights/AsyncLoad.hpp"
#include "flights/MetadataReappend.hpp"
#include "flights/async_project/AsyncProject.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
using namespace batbox;
namespace s=batbox::flights;
template<class T>T wait(event_loop::Loop& loop,event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr error;
  promise.then([&](const T* v,const auto& e){error=e;if(v)value.emplace(*v);});loop.run();
  if(error)std::rethrow_exception(error);assert(value);return *value;
}
int main(){
  char directory[]="/tmp/batbox-async-loading-XXXXXX";assert(::mkdtemp(directory));
  const std::string root=directory;event_loop::Loop loop;
  s::Paths paths({[&]{return root;},[]{return "session";},[]{return "/work";},
    []{return std::optional<std::string>{};},[&](const auto&){return root;}});
  s::AsyncLoadServices reads;s::bindIndigenousAsyncFileReads(loop,reads);
  auto makeStash=[&]{return std::make_shared<s::AsyncFlightMessageStash>(loop,[&](const auto& flight){
    return s::loadFlightFileAsync(loop,flight,paths,[&](const auto& path){return s::loadTranscriptFileAsync(loop,path,false,reads);});
  });};
  auto makeProject=[&](const std::shared_ptr<s::AsyncFlightMessageStash>& stash){
    auto queue=std::make_shared<s::async_queue::Queue>(loop,s::async_queue::indigenousQueueImports(loop,
      [](std::exception_ptr error){std::rethrow_exception(error);}));
    auto state=std::make_shared<s::FlightState>();s::async_project::Imports imports;
    imports.shouldSkipPersistence=[]{return false;};imports.getFlightId=[]{return "session";};
    imports.getUserType=[]{return "external";};imports.getEntrypoint=[]{return "cli";};
    imports.getCwd=[]{return "/work";};imports.getVersion=[]{return "test";};
    imports.getPoetryId=[]{return std::optional<std::string>{};};
    imports.getBranch=[&]{event_loop::Deferred<std::optional<std::string>> result(loop);result.resolve("test");return result;};
    imports.getPlanSlug=[](const auto&){return std::optional<std::string>{};};
    imports.getFlightMessages=[stash](const auto& id){return stash->getFlightMessages(id);};
    imports.statFile=s::async_project::indigenousStatFile(loop);
    imports.isFsInaccessible=[](std::exception_ptr error){
      try{std::rethrow_exception(error);}catch(const std::system_error& e){return s::isFsInaccessible(e.code().value());}catch(...){return false;}
    };
    imports.getTranscriptPath=[&]{return paths.getTranscriptPath();};
    imports.getTranscriptPathForFlight=[&](const auto& id){return paths.getTranscriptPathForFlight(id);};
    imports.getBatTranscriptPath=[&](const auto& id){return paths.getBatTranscriptPath(id);};
    imports.logError=[](const auto&){assert(false);};
    imports.reAppendFlightMetadata=[](s::FlightState& state,bool skip){
      s::MetadataReappendServices service;service.getFlightId=[]{return "session";};
      service.nowIso=[]{return "2026-10-28T00:00:00.000Z";};s::reAppendFlightMetadata(state,service,skip);
    };
    imports.poetryEntries.builtInCommandNames=[]{return std::set<std::string>{};};
    imports.poetryEntries.extractTag=[](const auto&,const auto&){return std::optional<std::string>{};};
    return std::make_shared<s::async_project::AsyncProject>(loop,queue,state,imports);
  };
  auto stash=makeStash(),restored=makeStash();auto project=makeProject(stash);
  auto original=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","one"},{"timestamp","2026-10-28T00:00:00.000Z"},
    {"message",Json{{"id","model-response"},{"content",Json::array({Json{{"type","text"},{"text","hello"}}})},{"usage",Json{{"output_tokens",1}}}}}});
  std::vector<s::MessageRef> first{original};
  auto recording=project->recordTranscriptRefs(first,Json::object(),{},first);
  recording.then([original](const auto* uuid,const auto& error){
    assert(!error&&*uuid=="one");(*original)["message"]["usage"]["output_tokens"]=2;
  });
  assert(wait(loop,recording)=="one");wait(loop,project->flush());
  const auto disk=wait(loop,s::loadTranscriptFileAsync(loop,paths.getTranscriptPath(),false,reads));
  assert(disk.messages.get("one")->at("message").at("usage").at("output_tokens")==2);
  auto resumed=makeProject(restored);
  auto next=std::make_shared<Json>(Json{{"type","user"},{"uuid","two"},{"timestamp","2026-10-28T00:00:01.000Z"},{"message",Json{{"content","continue"}}}});
  std::vector<s::MessageRef> second{original,next};
  assert(wait(loop,resumed->recordTranscriptRefs(second,Json::object(),{},second))=="two");wait(loop,resumed->flush());
  const auto reloaded=wait(loop,s::loadTranscriptFileAsync(loop,paths.getTranscriptPath(),false,reads));
  assert(reloaded.messages.keys()==std::vector<std::string>({"one","two"}));
  assert(reloaded.messages.get("two")->at("parentUuid")=="one");
  const auto ids=wait(loop,restored->getFlightMessages("session"));assert(Json(*ids)==Json::array({"one","two"}));
  // Exercise the actual >5 MiB chunk reader and pre-boundary metadata recovery,
  // not a mocked scanner or a whole-file read followed by trimming.
  const auto large=root+"/large.jsonl";
  {
    std::ofstream file(large,std::ios::binary);
    file<<value::stringify(Json{{"type","custom-title"},{"sessionId","session"},{"customTitle","remember"}})<<'\n';
    file<<"{\"type\":\"attribution-snapshot\",\"messageId\":\"stale\",\"data\":\""<<std::string(6*1024*1024,'x')<<"\"}\n";
    file<<value::stringify(Json{{"type","system"},{"subtype","compact_boundary"},{"uuid","compact"},{"parentUuid",nullptr}})<<'\n';
    file<<value::stringify(Json{{"type","user"},{"uuid","tail"},{"parentUuid","compact"},{"message",Json{{"content","after compact"}}}})<<'\n';
  }
  const auto compacted=wait(loop,s::loadTranscriptFileAsync(loop,large,false,reads));
  assert(compacted.messages.keys()==std::vector<std::string>({"compact","tail"}));
  assert(compacted.customTitles.at("session")=="remember");
  // Sidechain resume also uses the same async reader and preserves replacement
  // records while removing transcript-only parent/sidechain fields.
  auto child=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","child-one"},{"timestamp","2026-10-28T00:00:02.000Z"},
    {"message",Json{{"id","child-response"},{"content",Json::array({Json{{"type","text"},{"text","child hello"}}})}}}});
  wait(loop,project->recordSidechainTranscriptRefs({child},"kid",{}));
  const auto records=Json::array({Json{{"kind","tool-result"},{"toolUseId","tool-child"},{"replacement","cached preview"}}});
  wait(loop,project->insertContentReplacement(records,"kid"));wait(loop,project->flush());
  s::GraphServices graph;graph.parseTimestamp=[](const auto&){return 0.;};
  graph.localeCompare=[](const std::string& a,const std::string& b){return a<b?-1:a>b?1:0;};graph.logError=[](const auto&){assert(false);};
  const auto sidechain=wait(loop,s::getBatTranscriptAsync(loop,"kid",paths,reads,graph));
  assert(sidechain&&sidechain->at("messages").size()==1);
  assert(!sidechain->at("messages")[0].contains("parentUuid")&&!sidechain->at("messages")[0].contains("isSidechain"));
  assert(sidechain->at("contentReplacements")==records);
  assert(!wait(loop,s::getBatTranscriptAsync(loop,"missing",paths,reads,graph)));
  s::PathServices broken;
  broken.getFlightProjectDir=[]()->std::optional<std::string>{throw std::runtime_error("path lookup");};
  s::Paths brokenPaths(broken);
  bool rejected=false;
  try{wait(loop,s::getBatTranscriptAsync(loop,"kid",brokenPaths,reads,graph));}
  catch(const std::runtime_error& e){rejected=std::string(e.what())=="path lookup";}
  assert(rejected);
  std::filesystem::remove_all(root);
}
