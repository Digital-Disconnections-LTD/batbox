#include "diagnostics/LogBrowserIndigenous.hpp"
#include "event_loop/Loop.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

int main(){
  namespace fs=std::filesystem;
  const auto directory=fs::temp_directory_path()/
    ("batbox-log-browser-"+std::to_string(::getpid()));
  fs::create_directories(directory);
  {
    std::ofstream first(directory/"sidechain-a.json");
    first<<R"([{"type":"user","message":{"content":"first prompt"},"timestamp":"2026-03-31T12:00:00.000Z"},{"type":"assistant","timestamp":"2026-03-31T12:01:00.000Z"}])";
    std::ofstream second(directory/"b.json");
    second<<R"([{"type":"user","message":{"content":"second prompt"},"timestamp":"2026-03-31T12:00:00.000Z"},{"type":"assistant","timestamp":"2026-03-31T12:02:00.000Z"}])";
  }
  batbox::event_loop::Loop loop;
  auto io=std::make_shared<batbox::diagnostics::IndigenousLogBrowserIO>(loop,2);
  auto services=batbox::diagnostics::indigenousLogBrowserServices(loop,io,
    [directory]{return directory.string();},
    [](const std::string& left,const std::string& right){
      return (fs::path(left)/right).string();},
    [](const std::string& text){return batbox::value::parse(text);},
    [](const std::string&){assert(false&&"Unexpected directory failure");});
  batbox::diagnostics::LogBrowser browser(std::move(services));
  auto hold=loop.hold();
  bool done=false;
  auto result=browser.loadErrorLogs();
  result.then([&](const batbox::Json* value,const std::exception_ptr& error){
    assert(!error);assert(value&&value->is_array()&&value->size()==2);
    assert((*value)[0]["firstPrompt"]=="second prompt");
    assert((*value)[1]["isSidechain"]==true);
    assert((*value)[0]["messageCount"]==2);
    done=true;hold.release();
  });
  loop.run();
  assert(done);
  io.reset();
  fs::remove_all(directory);
}
