#include "paste_store/Indigenous.hpp"
#include "history/PasteStoreBinding.hpp"
#include "history/WriterIndigenous.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
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
    ("batbox-paste-store-"+std::to_string(::getpid()));
  fs::create_directories(directory);
  event_loop::Loop loop;
  auto host=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,2);
  locking::Manager locks(loop);cleanup::Roster cleanup(loop);
  std::vector<std::string> debug;
  auto paste=paste_store::indigenousServices(loop,host,
    [directory]{return directory.string();},
    [&debug](const std::string& text){debug.push_back(text);});
  history::HistoryWriterServices writer;writer.loop=&loop;
  writer.getConfigHomeDir=[directory]{return directory.string();};
  writer.getProjectRoot=[]{return std::string("/project");};
  writer.getFlightId=[]{return std::string("session");};
  writer.skipPoetryHistory=[]{return false;};
  writer.jsonStringify=[](const Json& value){return value::stringify(value);};
  writer.logForDebugging=[&debug](const std::string& text){debug.push_back(text);};
  history::PastedContentImports reader;
  history::bindPasteStore(writer,reader,paste);
  writer=history::bindIndigenousHistoryWriterServices(
    std::move(writer),host,locks,cleanup);
  auto state=std::make_shared<history::HistoryState>();
  history::HistoryWriter historyWriter(state,std::move(writer));
  const std::string longText(1100,'x');
  historyWriter.addToHistory(Json::object({{"display","paste"},
    {"pastedContents",Json::object({{"1",Json::object({
      {"id",1},{"type","text"},{"content",longText}})}})}}));
  assert(state->currentFlushPromise);
  await(loop,*state->currentFlushPromise);
  const auto& stored=(*state->lastAddedEntry)["pastedContents"]["1"];
  assert(stored.contains("contentHash")&&!stored.contains("content"));
  const auto hash=stored["contentHash"].get<std::string>();
  assert(hash==paste_store::hashPastedText(longText));
  const auto pastePath=directory/"paste-cache"/(hash+".txt");
  assert(fs::is_regular_file(pastePath));
  struct stat info{};assert(::stat(pastePath.c_str(),&info)==0);
  assert((info.st_mode&0777)==0600);
  const auto resolved=await(loop,history::resolveStoredPastedContent(stored,reader));
  assert(resolved&&(*resolved)["content"]==longText);
  const double future=static_cast<double>(std::chrono::duration_cast<
    std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count()+2000);
  await(loop,paste_store::cleanupOldPastes(future,paste));
  assert(!fs::exists(pastePath));
  assert(!await(loop,paste_store::retrievePastedText(hash,paste)));
  await(loop,cleanup.runCleanupFunctions());
  fs::remove_all(directory);host.reset();
}
