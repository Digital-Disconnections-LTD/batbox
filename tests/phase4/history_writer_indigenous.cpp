#include "history/WriterIndigenous.hpp"
#include "event_loop/Loop.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  auto hold=loop.hold();std::optional<T> value;std::exception_ptr error;
  promise.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;else value=*result;hold.release();});
  loop.run();if(error)std::rethrow_exception(error);assert(value);return *value;
}
int main(){
  namespace fs=std::filesystem;
  using namespace batbox;
  const auto directory=fs::temp_directory_path()/
    ("batbox-history-writer-"+std::to_string(::getpid()));
  fs::create_directories(directory);
  event_loop::Loop loop;
  auto host=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,2);
  locking::Manager locks(loop);
  cleanup::Roster cleanup(loop);
  history::HistoryWriterServices services;services.loop=&loop;
  services.getConfigHomeDir=[directory]{return directory.string();};
  services.getProjectRoot=[]{return std::string("/project");};
  services.getFlightId=[]{return std::string("session");};
  services.skipPoetryHistory=[]{return false;};
  services.jsonStringify=[](const Json& value){return value::stringify(value);};
  int diagnostics=0;
  services.logForDebugging=[&](const std::string&){++diagnostics;};
  services.hashPastedText=[](const std::string&){return std::string("hash");};
  services.storePastedText=[&loop](const std::string&,const std::string&){
    event_loop::Deferred<event_loop::Unit> result(loop);result.resolve({});return result;};
  auto state=std::make_shared<history::HistoryState>();
  history::HistoryWriter writer(state,history::bindIndigenousHistoryWriterServices(
    std::move(services),host,locks,cleanup));
  writer.addToHistory("first");
  assert(state->currentFlushPromise);
  await(loop,*state->currentFlushPromise);
  assert(state->pendingEntries.empty());
  assert(state->lastAddedEntry);
  const auto file=directory/"history.jsonl";
  assert(fs::is_regular_file(file));
  struct stat info{};assert(::stat(file.c_str(),&info)==0);
  assert((info.st_mode&0777)==0600);
  {std::ifstream input(file);std::string line;std::getline(input,line);
    const auto parsed=value::parse(line);
    assert(parsed["display"]=="first"&&parsed["project"]=="/project");
    assert(parsed["sessionId"]=="session");}
  const auto stamp=(*state->lastAddedEntry)["timestamp"].get<double>();
  writer.removeLastFromHistory();
  assert(state->skippedTimestamps.contains(stamp));
  writer.addToHistory("second");
  await(loop,*state->currentFlushPromise);
  await(loop,cleanup.runCleanupFunctions());
  assert(diagnostics==0);
  {std::ifstream input(file);std::string line;int count=0;
    while(std::getline(input,line))++count;
    assert(count==2);}
  fs::remove_all(directory);host.reset();
}
