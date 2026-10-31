#include "history/Views.hpp"
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
    ("batbox-history-views-"+std::to_string(::getpid()));
  fs::create_directories(directory);
  {std::ofstream file(directory/"history.jsonl");
    file<<R"({"display":"other","project":"/p","sessionId":"other","timestamp":1,"pastedContents":{}})"<<'\n';
    file<<R"({"display":"current","project":"/p","sessionId":"self","timestamp":2,"pastedContents":{}})"<<'\n';}
  event_loop::Loop loop;
  auto host=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,2);
  auto bytes=fs_operations::indigenousNodeFsBytesImports(loop,host);
  auto state=std::make_shared<history::HistoryState>();
  state->pendingEntries.push_back(std::make_shared<Json>(value::parse(
    R"({"display":"pending","project":"/p","sessionId":"self","timestamp":3,"pastedContents":{"1":{"id":1,"type":"text","contentHash":"h"}}})")));
  auto reader=history::indigenousHistoryReaderImports(loop,bytes,
    []{return std::string("self");},
    [directory]{return directory.string();},
    [](const std::string& line){return value::parse(line);},
    [](const std::string&){});
  int retrievals=0;
  history::PastedContentImports pastes;
  pastes.loop=&loop;
  pastes.retrievePastedText=[&](const std::string& hash){
    assert(hash=="h");++retrievals;
    event_loop::Deferred<std::optional<std::string>> result(loop);
    result.resolve(std::string("stored"));return result;};
  auto views=history::indigenousHistoryViewImports(loop,state,reader,pastes,
    []{return std::string("/p");},[]{return std::string("self");});
  {
    history::TimestampedHistoryCursor cursor(views);
    auto first=await(loop,cursor.next());
    assert(first&&first->display=="pending"&&retrievals==0);
    auto resolved=await(loop,first->resolve());
    assert(resolved["pastedContents"]["1"]["content"]=="stored");
    assert(retrievals==1);
    await(loop,cursor.close());
  }
  {
    history::GroupedHistoryCursor cursor(views);
    assert(await(loop,cursor.next())->at("display")=="pending");
    assert(await(loop,cursor.next())->at("display")=="current");
    assert(await(loop,cursor.next())->at("display")=="other");
    assert(!await(loop,cursor.next()));
  }
  fs::remove_all(directory);host.reset();
}
