#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  auto hold=loop.hold();
  std::optional<T> value;
  std::exception_ptr error;
  promise.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;else value=*result;
    hold.release();
  });
  loop.run();
  if(error)std::rethrow_exception(error);
  assert(value);return *value;
}

int main(){
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/
    ("batbox-node-fs-async-"+std::to_string(::getpid()));
  batbox::event_loop::Loop loop;
  auto host=std::make_shared<batbox::fs_operations::IndigenousAsyncFsHost>(loop,2);
  batbox::fs_operations::NodeFsAsync node(
    batbox::fs_operations::indigenousNodeFsAsyncImports(loop,host));
  const auto directory=(root/"nested").string();
  await(loop,node.mkdir(directory,batbox::Json::object({{"mode",0700}})));
  assert(fs::is_directory(directory));
  const auto file=(root/"nested"/"one.txt").string();
  {std::ofstream out(file);out<<"hello";}
  assert(await(loop,node.stat(file))["size"]==5);
  assert(await(loop,node.readFile(file,"utf8"))=="hello");
  const auto names=await(loop,node.readdir(directory));
  assert(names.size()==1&&names[0]["name"]=="one.txt");
  const auto renamed=(root/"nested"/"two.txt").string();
  await(loop,node.rename(file,renamed));
  assert(fs::exists(renamed));
  await(loop,node.unlink(renamed));
  await(loop,node.rmdir(directory));
  await(loop,node.rm(root.string(),batbox::Json::object({{"recursive",true}})));
  assert(!fs::exists(root));
  host.reset();
}
