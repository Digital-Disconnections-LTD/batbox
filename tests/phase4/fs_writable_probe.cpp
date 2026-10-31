#include "fs_operations/IndigenousWritableStream.hpp"
#include "event_loop/Loop.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

int main(){
  namespace fs=std::filesystem;
  using namespace batbox;
  using namespace batbox::fs_operations;
  Json input;std::cin>>input;
  const auto path=fs::temp_directory_path()/
    ("batbox-fs-write-probe-"+std::to_string(::getpid()));
  const auto target=input.value("invalid",false)?path/"missing":path;
  event_loop::Loop loop;
  auto host=std::make_shared<IndigenousAsyncFsHost>(loop,2);
  FsImplementation implementation;
  bindIndigenousCreateWriteStream(implementation,loop,host);
  auto stream=createWriteStream(implementation,target.string());
  Json events=Json::array(),returns=Json::array();bool drained=false;
  stream->onFinish([&]{events.push_back("finish");});
  stream->onDrain([&]{events.push_back("drain");drained=true;});
  stream->onError([&](const std::exception_ptr&){events.push_back("error");});
  stream->onClose([&]{events.push_back("close");});
  const auto chunks=input.value("chunks",Json::array());
  if(input.value("waitDrainAfterAll",false)){
    bool blocked=false;
    for(const auto& chunk:chunks){
      const bool accepted=stream->write(chunk.get<std::string>());
      returns.push_back(accepted);blocked|=!accepted;
    }
    if(blocked){loop.run();assert(drained);}
  }else if(input.value("waitDrain",false)&&!chunks.empty()){
    const bool first=stream->write(chunks[0].get<std::string>());
    returns.push_back(first);
    if(!first){loop.run();assert(drained);}
    for(std::size_t i=1;i<chunks.size();++i)
      returns.push_back(stream->write(chunks[i].get<std::string>()));
  }else for(const auto& chunk:chunks)
    returns.push_back(stream->write(chunk.get<std::string>()));
  stream->end();loop.run();
  Json contents=nullptr;
  if(fs::exists(path)){
    std::ifstream file(path,std::ios::binary);
    const std::string value(std::istreambuf_iterator<char>(file),{});
    contents=value::decodeUtf8(value);fs::remove(path);
  }
  std::cout<<value::stringify(Json::object({{"returns",returns},
    {"events",events},{"contents",contents}}))<<'\n';
  host.reset();
}
