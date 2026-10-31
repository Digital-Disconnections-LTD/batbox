#include <cstdlib>
#include "cli/headless/UnresolvedGadgetUse.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace batbox;
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;
  std::exception_ptr error;
  pending.then([&](const T* result,const auto& failure){
    error=failure;if(result)value=*result;
  });
  loop.run();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Unsettled transcript lookup");
  return *value;
}
int main(){
  char directory[]="/tmp/batbox-unresolved-tool-XXXXXX";
  if(!::mkdtemp(directory))throw std::runtime_error("mkdtemp failed");
  const std::string base=directory;
  event_loop::Loop loop;
  flights::Paths paths({[&]{return base;},[]{return "session";},
    []{return "/work";},[]{return std::optional<std::string>{};},
    [&](const auto&){return base;}});
  const auto path=paths.getTranscriptPath();
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  flights::AsyncLoadServices reads;
  flights::bindIndigenousAsyncFileReads(loop,reads);
  auto services=cli::headless::bindIndigenousUnresolvedGadgetUse(loop,paths,reads);
  auto lookup=[&](const auto& id){return cli::headless::findUnresolvedGadgetUse(loop,id,services);};
  const Json first={{"uuid","a"},{"type","assistant"},
    {"message",Json{{"id","a"},{"content",Json::array(
      {Json{{"type","tool_use"},{"id","t"},{"name","Read"},{"input",Json::object()}}})}}}};
  const Json second={{"uuid","b"},{"parentUuid","a"},{"type","user"},
    {"message",Json{{"content",Json::array(
      {Json{{"type","tool_result"},{"tool_use_id","t"},{"content","ok"}}})}}}};
  {std::ofstream file(path,std::ios::binary);
   if(!file)throw std::runtime_error("open failed");
   file<<value::stringify(first)<<'\n';}
  if(await(loop,lookup("t"))!=first)throw std::runtime_error("Tool use not found");
  {std::ofstream file(path,std::ios::binary|std::ios::app);
   if(!file)throw std::runtime_error("open failed");
   file<<value::stringify(second)<<'\n';}
  if(!await(loop,lookup("t")).is_null())
    throw std::runtime_error("Tool result did not resolve use");
  std::filesystem::remove_all(base);
}
