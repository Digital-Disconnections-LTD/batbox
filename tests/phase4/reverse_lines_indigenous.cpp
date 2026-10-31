#include "fs_operations/ReverseLines.hpp"
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
    ("batbox-reverse-lines-"+std::to_string(::getpid()));
  const std::string longLine=std::string(4095,'x')+"é";
  {std::ofstream file(path,std::ios::binary);file<<longLine<<"\nlast\n";}
  batbox::event_loop::Loop loop;
  auto host=std::make_shared<IndigenousAsyncFsHost>(loop,2);
  const auto imports=indigenousNodeFsBytesImports(loop,host);
  {
    ReverseLinesCursor cursor(imports,path.string());
    assert(await(loop,cursor.next())==std::optional<std::string>("last"));
    assert(await(loop,cursor.next())==std::optional<std::string>(longLine));
    assert(!await(loop,cursor.next()));
  }
  {
    ReverseLinesCursor cursor(imports,path.string());
    assert(await(loop,cursor.next())==std::optional<std::string>("last"));
    await(loop,cursor.close());
    assert(!await(loop,cursor.next()));
  }
  fs::remove(path);host.reset();
}
