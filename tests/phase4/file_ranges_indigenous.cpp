#include "fs_operations/FileRanges.hpp"
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
    ("batbox-file-ranges-"+std::to_string(::getpid()));
  {std::ofstream file(path,std::ios::binary);file<<"abc\nxyz";}
  batbox::event_loop::Loop loop;
  auto host=std::make_shared<IndigenousAsyncFsHost>(loop,2);
  const auto imports=indigenousNodeFsBytesImports(loop,host);
  const auto prefix=await(loop,readFileRange(imports,path.string(),0,3));
  assert(prefix&&prefix->at("content")=="abc");
  assert(prefix->at("bytesRead")==3&&prefix->at("bytesTotal")==7);
  const auto middle=await(loop,readFileRange(imports,path.string(),2,3));
  assert(middle&&middle->at("content")=="c\nx");
  const auto beyond=await(loop,readFileRange(imports,path.string(),7,3));
  assert(!beyond);
  const auto tail=await(loop,tailFile(imports,path.string(),3));
  assert(tail.at("content")=="xyz"&&tail.at("bytesTotal")==7);
  const auto zero=await(loop,tailFile(imports,path.string(),0));
  assert(zero.at("content")==""&&zero.at("bytesRead")==0);
  fs::remove(path);host.reset();
}
