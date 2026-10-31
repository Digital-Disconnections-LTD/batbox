#include <cstdlib>
#include "gadgets/bash/AsyncBashImage.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unistd.h>

using namespace batbox::bash_gadget;
namespace fs=std::filesystem;
int main(){
  char temporary[]="/tmp/batbox-bash-image-XXXXXX";
  assert(::mkdtemp(temporary));
  const fs::path root=temporary;
  try{
    constexpr const char* pixel="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/sXcAAAAASUVORK5CYII=";
    const auto path=root/"pixel.txt";
    {std::ofstream file(path);file<<pixel;}
    {
      batbox::event_loop::Loop loop;
      auto ops=indigenousBashImageOps(loop);
      std::optional<std::string> actual;
      auto result=resizeShellImageOutputAsync(loop,"ignored",path.string(),{},ops);
      result.then([&](const std::optional<std::string>* value,const std::exception_ptr& error){
        assert(loop.isOwnerThread());if(error)std::rethrow_exception(error);actual=*value;
      });
      loop.run();
      assert(actual&&actual->rfind("data:image/png;base64,",0)==0);
    }
    const auto large=root/"large.txt";
    {std::ofstream file(large);file<<pixel;}
    fs::resize_file(large,20ull*1024*1024+1);
    {
      batbox::event_loop::Loop loop;
      auto ops=indigenousBashImageOps(loop);
      bool completed=false;
      auto result=resizeShellImageOutputAsync(loop,"ignored",large.string(),{},ops);
      result.then([&](const std::optional<std::string>* value,const std::exception_ptr& error){
        assert(!error);assert(!*value);completed=true;
      });
      loop.run();assert(completed);
    }
    fs::remove_all(root);
    std::cout<<"native Bash image FS/media binding passed\n";
  }catch(...){fs::remove_all(root);throw;}
}
