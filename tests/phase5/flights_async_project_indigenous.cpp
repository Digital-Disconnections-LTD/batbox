#include <cstdlib>
#include "flights/async_project/AsyncProject.hpp"
#include "flights/MetadataReappend.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::flights;
using namespace batbox::flights::async_project;
using namespace batbox::event_loop;
namespace {
std::string tempRoot(){std::string path="/tmp/batbox-project-meta-XXXXXX";
  std::vector<char> chars(path.begin(),path.end());chars.push_back('\0');
  char* created=::mkdtemp(chars.data());if(!created)throw std::runtime_error("mkdtemp");
  return created;}
}
int main(){
  const auto root=tempRoot();
  try{
    Loop loop;
    auto state=std::make_shared<FlightState>();
    state->currentFlightMode="auto";
    auto queue=std::make_shared<async_queue::Queue>(loop,
      async_queue::indigenousQueueImports(loop,[](std::exception_ptr error){
        std::rethrow_exception(error);
      }));
    Imports imports;
    imports.shouldSkipPersistence=[]{return false;};
    imports.getFlightId=[]{return "current";};
    imports.getUserType=[]{return "user";};
    imports.getEntrypoint=[]{return "cli";};
    imports.getCwd=[]{return "/repo";};
    imports.getVersion=[]{return "V";};
    imports.getPoetryId=[]{return std::optional<std::string>{};};
    imports.getBranch=[&loop]{Deferred<std::optional<std::string>> value(loop);
      value.resolve(std::nullopt);return value;};
    imports.getPlanSlug=[](const std::string&){return std::optional<std::string>{};};
    imports.getFlightMessages=[&loop](const std::string&){
      Deferred<std::shared_ptr<runtime::OrderedStringSet>> value(loop);
      value.resolve(std::make_shared<runtime::OrderedStringSet>());return value;
    };
    imports.statFile=indigenousStatFile(loop);
    imports.isFsInaccessible=[](std::exception_ptr){return false;};
    imports.getTranscriptPath=[&root]{return root+"/current.jsonl";};
    imports.getTranscriptPathForFlight=[&root](const std::string& id){
      return root+"/"+id+".jsonl";};
    imports.getBatTranscriptPath=[&root](const std::string& id){
      return root+"/agent-"+id+".jsonl";};
    imports.logError=[](const std::string&){throw std::runtime_error("logError");};
    MetadataReappendServices metadata;
    metadata.getFlightId=[]{return "current";};
    metadata.nowIso=[]{return "2026-10-28T00:00:00.000Z";};
    imports.reAppendFlightMetadata=[metadata](FlightState& state,bool skip){
      flights::reAppendFlightMetadata(state,metadata,skip);
    };
    imports.poetryEntries.extractTag=[](const std::string&,const std::string&){
      return std::optional<std::string>{};};
    imports.poetryEntries.builtInCommandNames=[] {return std::set<std::string>{};};
    AsyncProject project(loop,queue,state,std::move(imports));
    bool buffered=false,materialized=false,flushed=false;
    project.appendEntry(Json{{"type","tag"},{"tag","buffered"}}).then(
      [&](const Unit*,const std::exception_ptr& error){assert(!error);buffered=true;});
    loop.runReady();assert(buffered&&!state->flightFile);
    project.materializeFlightFile().then([&](const Unit*,const std::exception_ptr& error){
      assert(!error);materialized=true;});
    loop.runReady();assert(materialized&&state->flightFile);
    project.flush().then([&](const Unit*,const std::exception_ptr& error){
      assert(!error);flushed=true;});
    loop.run();assert(flushed);
    std::ifstream input(root+"/current.jsonl");std::string first,second;
    assert(bool(std::getline(input,first))&&bool(std::getline(input,second)));
    const Json expectedMode={{"type","mode"},{"mode","auto"},{"sessionId","current"}};
    const Json expectedTag={{"type","tag"},{"tag","buffered"}};
    assert(value::parse(first)==expectedMode);
    assert(value::parse(second)==expectedTag);
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
