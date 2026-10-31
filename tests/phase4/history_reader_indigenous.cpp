#include "history/ReaderIndigenous.hpp"
#include "fs_operations/NodeFsBytesIndigenous.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
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
    ("batbox-history-reader-"+std::to_string(::getpid()));
  fs::create_directories(directory);
  const auto path=directory/"history.jsonl";
  {std::ofstream file(path);
    file<<R"({"display":"old","sessionId":"other","timestamp":1})"<<'\n';
    file<<"BAD\n";
    file<<R"({"display":"skip","sessionId":"self","timestamp":7})"<<'\n';
    file<<R"({"display":"new","sessionId":"self","timestamp":8})"<<'\n';}
  event_loop::Loop loop;
  auto host=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,2);
  auto bytes=fs_operations::indigenousNodeFsBytesImports(loop,host);
  std::vector<std::string> debug;
  auto imports=history::indigenousHistoryReaderImports(loop,bytes,
    []{return std::string("self");},
    [directory]{return directory.string();},
    [](const std::string& line){return value::parse(line);},
    [&debug](const std::string& message){debug.push_back(message);});
  auto state=std::make_shared<history::HistoryState>();
  state->pendingEntries.push_back(std::make_shared<Json>(Json::object(
    {{"display","pending"},{"timestamp",9},{"sessionId","self"}})));
  state->skippedTimestamps.insert(7);
  {
    history::HistoryLogReader reader(state,imports);
    assert((**await(loop,reader.next()))["display"]=="pending");
    assert((**await(loop,reader.next()))["display"]=="new");
    assert((**await(loop,reader.next()))["display"]=="old");
    assert(!await(loop,reader.next()));
  }
  assert(debug.size()==1);
  {
    history::HistoryLogReader reader(state,imports);
    assert((**await(loop,reader.next()))["display"]=="pending");
    assert((**await(loop,reader.next()))["display"]=="new");
    await(loop,reader.close());
    assert(!await(loop,reader.next()));
  }
  fs::remove(path);
  {
    history::HistoryLogReader reader(state,imports);
    assert((**await(loop,reader.next()))["display"]=="pending");
    assert(!await(loop,reader.next()));
  }
  fs::remove_all(directory);host.reset();
}
