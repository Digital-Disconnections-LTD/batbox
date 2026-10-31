#include "fs_operations/IndigenousWritableStream.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

int main(){
  namespace fs=std::filesystem;
  using namespace batbox::fs_operations;
  const auto base=fs::temp_directory_path()/
    ("batbox-native-write-lifecycle-"+std::to_string(::getpid()));
  batbox::event_loop::Loop loop;
  auto host=std::make_shared<IndigenousAsyncFsHost>(loop,2);
  {
    std::vector<std::string> events;
    auto stream=createIndigenousWritableStream(loop,host,(base.string()+"-abort"));
    stream->onClose([&]{events.push_back("close");});
    stream->onFinish([&]{events.push_back("finish");});
    stream->onError([&](const std::exception_ptr&){events.push_back("error");});
    stream->write("discard");stream->abort();loop.run();
    assert(events==std::vector<std::string>{"close"});
    fs::remove(base.string()+"-abort");
  }
  {
    std::vector<std::string> events;
    auto stream=createIndigenousWritableStream(loop,host,(base.string()+"-after"));
    stream->onClose([&]{events.push_back("close");});
    stream->onFinish([&]{events.push_back("finish");});
    stream->onError([&](const std::exception_ptr&){events.push_back("error");});
    stream->write("ok");stream->end();
    assert(!stream->write("late"));loop.run();
    assert((events==std::vector<std::string>{"error","close"}));
    fs::remove(base.string()+"-after");
  }
  host.reset();
}
