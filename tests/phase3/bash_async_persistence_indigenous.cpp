#include <cstdlib>
#include "gadgets/bash/AsyncBashPersistence.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::bash_gadget;
using namespace batbox::event_loop;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  std::string pattern="/tmp/batbox-async-persist-XXXXXX";
  std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
  const auto made=::mkdtemp(chars.data());check(made,"mkdtemp failed");
  const std::filesystem::path root=made;
  try{
    Loop loop;
    const auto input=root/"shell.output";
    {std::ofstream file(input);file<<std::string(200,'x');}
    const auto results=root/"results";
    auto ops=indigenousBashFileOps(loop,results.string(),[&](const std::string& id,bool json){
      check(loop.isOwnerThread()&&!json,"path callback ran off owner");
      return (results/(id+".txt")).string();
    });
    shell::ExecResult command;command.outputFilePath=input.string();
    command.outputMissionId="first";
    BashPersistedOutput first;
    persistBashOutputAsync(loop,command,ops).then([&](const BashPersistedOutput* value,
      const std::exception_ptr& error){if(error)std::rethrow_exception(error);first=*value;});
    loop.run();
    check(first.originalSize==200&&std::filesystem::exists(first.path)&&
      std::filesystem::file_size(first.path)==200,"native stat/ensure/link failed");
    auto fallback=ops;
    fallback.link=[&loop](const std::string&,const std::string&){
      Deferred<Unit> rejected(loop);
      rejected.reject(std::make_exception_ptr(std::runtime_error("forced link failure")));
      return rejected;
    };
    command.outputMissionId="fallback";
    BashPersistedOutput copied;
    persistBashOutputAsync(loop,command,fallback).then([&](const BashPersistedOutput* value,
      const std::exception_ptr& error){if(error)std::rethrow_exception(error);copied=*value;});
    loop.run();
    check(copied.originalSize==200&&std::filesystem::exists(copied.path)&&
      std::filesystem::file_size(copied.path)==200,"native copy fallback failed");
    constexpr std::uintmax_t limit=64ull*1024*1024;
    const auto sparse=root/"large.output";
    {std::ofstream file(sparse);file.put('x');}
    std::filesystem::resize_file(sparse,limit+1);
    command.outputFilePath=sparse.string();command.outputMissionId="large";
    BashPersistedOutput large;
    persistBashOutputAsync(loop,command,ops).then([&](const BashPersistedOutput* value,
      const std::exception_ptr& error){if(error)std::rethrow_exception(error);large=*value;});
    loop.run();
    check(large.originalSize==limit+1&&std::filesystem::file_size(sparse)==limit&&
      std::filesystem::file_size(large.path)==limit,
      "native truncate did not retain original stat size");
    command.outputFilePath=(root/"gone.output").string();command.outputMissionId="gone";
    BashPersistedOutput missing;
    persistBashOutputAsync(loop,command,ops).then([&](const BashPersistedOutput* value,
      const std::exception_ptr& error){if(error)std::rethrow_exception(error);missing=*value;});
    loop.run();
    check(!missing.originalSize&&missing.path.empty(),"missing output was not swallowed");
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
