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
  using namespace batbox::fs_operations;
  const auto path=fs::temp_directory_path()/
    ("batbox-node-fs-bytes-"+std::to_string(::getpid()));
  {std::ofstream file(path,std::ios::binary);file<<"abcdef";}
  batbox::event_loop::Loop loop;
  auto host=std::make_shared<IndigenousAsyncFsHost>(loop,2);
  NodeFsBytes wrapper(indigenousNodeFsBytesImports(loop,host));
  const auto all=await(loop,wrapper.readFileBytes(path.string()));
  assert(std::string(all.begin(),all.end())=="abcdef");
  const auto first=await(loop,wrapper.readFileBytes(path.string(),3));
  assert(std::string(first.begin(),first.end())=="abc");
  const auto beyond=await(loop,wrapper.readFileBytes(path.string(),20));
  assert(std::string(beyond.begin(),beyond.end())=="abcdef");
  const auto zero=await(loop,wrapper.readFileBytes(path.string(),0));
  assert(zero.empty());
  fs::remove(path);
  bool failed=false;
  try{(void)await(loop,wrapper.readFileBytes(path.string(),3));}
  catch(const std::system_error&){failed=true;}
  assert(failed);
  host.reset();
}
