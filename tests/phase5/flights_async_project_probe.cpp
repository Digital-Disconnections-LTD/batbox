#include <cstdlib>
#include "flights/async_project/AsyncProject.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::flights;
using namespace batbox::flights::async_project;
using namespace batbox::event_loop;
namespace {
std::string tempRoot(){std::string value="/tmp/batbox-async-project-XXXXXX";
  std::vector<char> chars(value.begin(),value.end());chars.push_back('\0');
  const auto created=::mkdtemp(chars.data());if(!created)throw std::runtime_error("mkdtemp");
  return created;}
Json strings(const std::vector<std::string>& values){Json output=Json::array();
  for(const auto& value:values)output.push_back(value);return output;}
struct Fixture {
  Loop loop;
  std::string root=tempRoot();
  std::vector<std::string> trace;
  std::vector<Deferred<std::optional<std::string>>> branches;
  std::vector<Deferred<std::shared_ptr<runtime::OrderedStringSet>>> sets;
  std::shared_ptr<async_queue::Queue> queue;
  std::shared_ptr<FlightState> state=std::make_shared<FlightState>();
  std::unique_ptr<AsyncProject> project;
  Fixture(){
    queue=std::make_shared<async_queue::Queue>(loop,async_queue::indigenousQueueImports(
      loop,[](std::exception_ptr error){std::rethrow_exception(error);}));
    Imports imports;
    imports.shouldSkipPersistence=[]{return false;};
    imports.getFlightId=[]{return "current";};
    imports.getUserType=[]{return "user";};
    imports.getEntrypoint=[]{return "cli";};
    imports.getCwd=[]{return "/repo";};
    imports.getVersion=[]{return "V";};
    imports.getPoetryId=[]{return std::optional<std::string>{};};
    imports.getBranch=[this]{trace.push_back("branch");
      Deferred<std::optional<std::string>> next(loop);branches.push_back(next);return next;};
    imports.getPlanSlug=[](const std::string&){return std::optional<std::string>{};};
    imports.getFlightMessages=[this](const std::string& flight){
      trace.push_back("messages:"+flight);
      Deferred<std::shared_ptr<runtime::OrderedStringSet>> next(loop);sets.push_back(next);return next;};
    auto stat=indigenousStatFile(loop);
    imports.statFile=[this,stat](const std::string& file){
      const auto name=std::filesystem::path(file).filename().string();
      trace.push_back("stat:"+name);
      if(name=="error.jsonl"){
        Completion result(loop);result.reject(std::make_exception_ptr(std::runtime_error("")));
        return result;
      }
      return stat(file);
    };
    imports.isFsInaccessible=[](std::exception_ptr error){
      try{std::rethrow_exception(error);}catch(const std::system_error& value){
        return value.code().value()==ENOENT||value.code().value()==ENOTDIR||
          value.code().value()==EACCES||value.code().value()==EPERM||
          value.code().value()==ELOOP;
      }catch(...){return false;}
    };
    imports.getTranscriptPath=[this]{return root+"/current.jsonl";};
    imports.getTranscriptPathForFlight=[this](const std::string& flight){
      return root+"/"+flight+".jsonl";};
    imports.getBatTranscriptPath=[this](const std::string& bat){
      return root+"/agent-"+bat+".jsonl";};
    imports.logError=[this](const std::string& error){trace.push_back("error:"+error);};
    imports.reAppendFlightMetadata=[this](FlightState&,bool){trace.push_back("reappend");};
    imports.poetryEntries.extractTag=[](const std::string&,const std::string&){
      return std::optional<std::string>{};};
    imports.poetryEntries.builtInCommandNames=[] {return std::set<std::string>{};};
    project=std::make_unique<AsyncProject>(loop,queue,state,std::move(imports));
  }
  ~Fixture(){project.reset();queue.reset();std::filesystem::remove_all(root);}
  void tick(){loop.runReady();}
  void settleWrites(){bool done=false;project->flush().then([&](const Unit*,
    const std::exception_ptr& error){if(error)std::rethrow_exception(error);done=true;});
    loop.run();if(!done)throw std::runtime_error("flush did not complete");}
  Json entries(const std::string& name="current.jsonl")const{
    Json output=Json::array();std::ifstream input(root+"/"+name);std::string line;
    while(std::getline(input,line))if(!line.empty())output.push_back(Json{
      {"file",name},{"entry",value::parse(line)}});
    return output;
  }
};
Json materializeChain(){Fixture s;
  bool buffered=false;s.project->appendEntry(Json{{"type","tag"},{"tag","queued"}}).then(
    [&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);buffered=true;});
  s.tick();if(!buffered)throw std::runtime_error("buffering did not complete");
  auto message=std::make_shared<Json>(Json{{"type","user"},{"uuid","u1"},
    {"message",Json{{"role","user"},{"content","hello"}}}});
  bool done=false;s.project->insertMessageChainRefs({message}).then(
    [&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);
      done=true;s.trace.push_back("done");});
  s.tick();Json beforeBranch={{"trace",strings(s.trace)},
    {"queued",s.state->pendingEntries.size()==0?1:0},{"done",done}};
  s.branches.at(0).resolve(std::optional<std::string>("main"));s.tick();
  Json beforeSet={{"trace",strings(s.trace)},{"queued",1},{"done",done}};
  s.sets.at(0).resolve(std::make_shared<runtime::OrderedStringSet>());s.tick();
  s.settleWrites();
  return Json{{"beforeBranch",beforeBranch},{"beforeSet",beforeSet},
    {"after",Json{{"trace",strings(s.trace)},{"entries",s.entries()},
      {"lastPrompt",s.state->currentFlightLastPoetry.value_or("")},{"done",done}}}};
}
Json foreignStatStash(){Fixture s;
  {std::ofstream file(s.root+"/other.jsonl");}
  bool first=false;s.project->appendEntry(Json{{"type","tag"},{"tag","one"}},"other")
    .then([&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);
      first=true;s.trace.push_back("first");});
  Json pending={{"trace",strings(s.trace)},{"queued",0},{"first",first}};
  s.loop.run();
  bool second=false;s.project->appendEntry(Json{{"type","tag"},{"tag","two"}},"other")
    .then([&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);
      second=true;});
  s.tick();if(!first||!second)throw std::runtime_error("foreign appends incomplete");
  s.settleWrites();
  return Json{{"pending",pending},{"after",Json{{"trace",strings(s.trace)},
    {"entries",s.entries("other.jsonl")},{"statCalls",1}}}};
}
Json recordPrefix(){Fixture s;s.state->flightFile=s.root+"/current.jsonl";
  auto prefix=std::make_shared<Json>(Json{{"type","user"},{"uuid","old"},
    {"message",Json{{"role","user"},{"content","old"}}}});
  auto fresh=std::make_shared<Json>(Json{{"type","user"},{"uuid","new"},
    {"message",Json{{"role","user"},{"content","new"}}}});
  std::optional<std::string> returned;
  s.project->recordTranscriptRefs({prefix,fresh},Json{{"teamName","team"}},
    std::nullopt,{prefix,fresh}).then([&](const std::optional<std::string>* value,
    const std::exception_ptr& error){if(error)std::rethrow_exception(error);
    returned=*value;s.trace.push_back("returned:"+value->value_or("null"));});
  s.tick();s.sets.at(0).resolve(std::make_shared<runtime::OrderedStringSet>(
    std::initializer_list<std::string>{"old"}));s.tick();
  s.branches.at(0).resolve(std::optional<std::string>("main"));s.tick();
  s.sets.at(1).resolve(std::make_shared<runtime::OrderedStringSet>(
    std::initializer_list<std::string>{"old"}));s.tick();s.settleWrites();
  return Json{{"trace",strings(s.trace)},{"entries",s.entries()},
    {"returned",returned.value_or("")}};
}
Json sidechain(){Fixture s;s.state->flightFile=s.root+"/current.jsonl";
  auto message=std::make_shared<Json>(Json{{"type","user"},{"uuid","side"},
    {"message",Json{{"role","user"},{"content","side"}}}});
  bool done=false;s.project->recordSidechainTranscriptRefs({message},"agent1","parent").then(
    [&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);
      done=true;s.trace.push_back("done");});
  s.tick();s.branches.at(0).resolve(std::optional<std::string>("main"));s.tick();
  s.sets.at(0).resolve(std::make_shared<runtime::OrderedStringSet>(
    std::initializer_list<std::string>{"side"}));s.tick();s.settleWrites();
  return Json{{"trace",strings(s.trace)},
    {"entries",s.entries("agent-agent1.jsonl")},{"done",done}};
}
Json delayedMessageBody(){Fixture s;s.state->flightFile=s.root+"/current.jsonl";
  auto message=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a1"},
    {"message",Json{{"role","assistant"},{"content","hi"},
      {"usage",Json{{"input_tokens",0}}}}}});
  bool done=false;s.project->insertMessageChainRefs({message}).then(
    [&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);
      done=true;});
  s.tick();s.branches.at(0).resolve(std::optional<std::string>("main"));s.tick();
  s.sets.at(0).resolve(std::make_shared<runtime::OrderedStringSet>());s.tick();
  {runtime::MessageLock lock(message);lock.value()["message"]["usage"]["input_tokens"]=7;}
  s.settleWrites();
  return Json{{"entries",s.entries()},{"done",done}};
}
Json foreignMissing(){Fixture s;bool done=false;
  s.project->appendEntry(Json{{"type","tag"},{"tag","missing"}},"missing")
    .then([&](const Unit*,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);done=true;s.trace.push_back("done");});
  s.loop.run();
  return Json{{"trace",strings(s.trace)},{"entries",Json::array()},{"done",done}};
}
Json foreignUnexpectedEmptyError(){Fixture s;bool rejected=false;
  s.project->appendEntry(Json{{"type","tag"},{"tag","error"}},"error")
    .then([&](const Unit*,const std::exception_ptr& error){
      if(error){rejected=true;s.trace.push_back("rejected");}
      else s.trace.push_back("done");});
  s.tick();
  return Json{{"trace",strings(s.trace)},{"entries",Json::array()},
    {"rejected",rejected}};
}
Json collapseRecords(){Fixture s;s.state->flightFile=s.root+"/current.jsonl";
  bool first=false,second=false;
  s.project->recordContextCollapseCommit(Json{{"collapseId","c1"},
    {"summaryUuid","u"},{"summaryContent","text"},{"summary","short"},
    {"firstArchivedUuid","a"},{"lastArchivedUuid","z"}}).then(
    [&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);
      first=true;});
  s.tick();if(!first)throw std::runtime_error("commit not appended");
  s.project->recordContextCollapseSnapshot(Json{{"staged",Json::array()},
    {"armed",false},{"lastSpawnTokens",3}}).then(
    [&](const Unit*,const std::exception_ptr& error){if(error)std::rethrow_exception(error);
      second=true;});
  s.tick();if(!second)throw std::runtime_error("snapshot not appended");
  s.settleWrites();return Json{{"entries",s.entries()}};
}
}
int main(){Json result={{"materializeChain",materializeChain()},
  {"foreignStatCache",foreignStatStash()},{"recordPrefix",recordPrefix()},
  {"sidechain",sidechain()},{"delayedMessageBody",delayedMessageBody()},
  {"foreignMissing",foreignMissing()},
  {"foreignUnexpectedEmptyError",foreignUnexpectedEmptyError()},
  {"collapseRecords",collapseRecords()}};
  std::cout<<value::stringify(result)<<'\n';}
