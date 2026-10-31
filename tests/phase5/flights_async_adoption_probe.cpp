#include "flights/async_project/AsyncProject.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace batbox;
using namespace batbox::flights;
using namespace batbox::flights::async_project;
using namespace batbox::event_loop;
namespace {
struct Fixture {
  Loop loop;
  std::shared_ptr<runtime::OrderedStringSet> recorded;
  std::shared_ptr<async_queue::Queue> queue;
  std::shared_ptr<FlightState> state=std::make_shared<FlightState>();
  std::unique_ptr<AsyncProject> project;
  explicit Fixture(runtime::OrderedStringSet uuids):
    recorded(std::make_shared<runtime::OrderedStringSet>(std::move(uuids))){
    async_queue::QueueImports filesystem;
    filesystem.appendFile=[this](const std::string&,const std::string&){
      Completion done(loop);done.resolve({});return done;};
    filesystem.mkdir=[this](const std::string&){Completion done(loop);done.resolve({});return done;};
    filesystem.onBackgroundError=[](std::exception_ptr error){std::rethrow_exception(error);};
    queue=std::make_shared<async_queue::Queue>(loop,std::move(filesystem));
    state->flightFile="current.jsonl";
    Imports imports;
    imports.shouldSkipPersistence=[]{return false;};
    imports.getFlightId=[]{return "current";};
    imports.getUserType=[]{return "user";};
    imports.getEntrypoint=[]{return "cli";};
    imports.getCwd=[]{return "/repo";};
    imports.getVersion=[]{return "V";};
    imports.getPoetryId=[]{return std::optional<std::string>{};};
    imports.getBranch=[this]{Deferred<std::optional<std::string>> value(loop);
      value.resolve(std::optional<std::string>("main"));return value;};
    imports.getPlanSlug=[](const std::string&){return std::optional<std::string>{};};
    imports.getFlightMessages=[this](const std::string&){
      Deferred<std::shared_ptr<runtime::OrderedStringSet>> value(loop);
      value.resolve(recorded);return value;};
    imports.statFile=[this](const std::string&){Completion value(loop);value.resolve({});return value;};
    imports.isFsInaccessible=[](std::exception_ptr){return false;};
    imports.getTranscriptPath=[]{return "current.jsonl";};
    imports.getTranscriptPathForFlight=[](const std::string&){return "other.jsonl";};
    imports.getBatTranscriptPath=[](const std::string&){return "agent.jsonl";};
    imports.logError=[](const std::string&){throw std::runtime_error("unexpected log");};
    imports.reAppendFlightMetadata=[](FlightState&,bool){};
    imports.poetryEntries.extractTag=[](const std::string&,const std::string&){
      return std::optional<std::string>{};};
    imports.poetryEntries.builtInCommandNames=[] {return std::set<std::string>{};};
    project=std::make_unique<AsyncProject>(loop,queue,state,std::move(imports));
  }
};
void ladder(Loop& loop,std::vector<std::string>& trace,int index){
  trace.push_back("tick"+std::to_string(index));
  if(index<12)loop.queueMicrotask([&loop,&trace,index]{ladder(loop,trace,index+1);});
}
template<class Start>Json run(Start start,runtime::OrderedStringSet recorded={}){
  Fixture fixture(std::move(recorded));std::vector<std::string> trace;
  start(fixture,trace);
  fixture.loop.queueMicrotask([&]{trace.push_back("sibling");});
  fixture.loop.queueMicrotask([&]{ladder(fixture.loop,trace,0);});
  fixture.loop.runReady();
  Json output=Json::array();for(const auto& item:trace)output.push_back(item);
  return output;
}
std::shared_ptr<Json> message(){return std::make_shared<Json>(Json{{"type","user"},
  {"uuid","u1"},{"message",Json{{"role","user"},{"content","hi"}}}});}
}
int main(){Json output=Json::object();
  output["trackWrite"]=run([](Fixture& f,std::vector<std::string>& trace){
    f.queue->trackWrite<int>([&]{Deferred<int> inner(f.loop);inner.resolve(7);return inner;})
      .then([&](const int*,const std::exception_ptr& error){
        trace.push_back(error?"error":"done");});
  });
  output["insertEmpty"]=run([](Fixture& f,std::vector<std::string>& trace){
    f.project->insertMessageChainRefs({}).then([&](const Unit*,const std::exception_ptr& error){
      trace.push_back(error?"error":"done");});
  });
  output["appendMetadata"]=run([](Fixture& f,std::vector<std::string>& trace){
    f.project->appendEntry(Json{{"type","tag"},{"tag","x"}}).then(
      [&](const Unit*,const std::exception_ptr& error){trace.push_back(error?"error":"done");});
  });
  output["appendMessage"]=run([](Fixture& f,std::vector<std::string>& trace){
    f.project->appendEntry(Json{{"type","user"},{"uuid","u1"}}).then(
      [&](const Unit*,const std::exception_ptr& error){trace.push_back(error?"error":"done");});
  });
  output["recordAllRecorded"]=run([](Fixture& f,std::vector<std::string>& trace){
    auto ref=message();f.project->recordTranscriptRefs({ref},Json::object(),std::nullopt,{ref})
      .then([&](const std::optional<std::string>*,const std::exception_ptr& error){
        trace.push_back(error?"error":"done");});
  },{"u1"});
  output["recordNew"]=run([](Fixture& f,std::vector<std::string>& trace){
    auto ref=message();f.project->recordTranscriptRefs({ref},Json::object(),std::nullopt,{ref})
      .then([&](const std::optional<std::string>*,const std::exception_ptr& error){
        trace.push_back(error?"error":"done");});
  });
  std::cout<<value::stringify(output)<<'\n';
}
