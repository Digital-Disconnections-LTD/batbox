#include "cleanup/Retention.hpp"
#include "runtime/JsonValue.hpp"
#include "shell/PosixPath.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <system_error>

using namespace batbox;
namespace {
bool contains(const Json& list,const std::string& value){
  for(const auto& item:list)if(item==value)return true;return false;
}
std::string message(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}catch(const std::exception& value){return value.what();}
}
Json evaluate(const Json& item){
  event_loop::Loop loop;Json logs=Json::array();
  auto files=item.value("files",Json::object());
  cleanup::RetentionServices services;
  services.joinPath=shell::joinPosixPath;
  services.parseDateMs=cleanup::indigenousLogDateMs;
  services.logError=[&](std::exception_ptr error){logs.push_back("log:"+message(error));};
  services.fs.readdir=[&](const std::string& dir){
    logs.push_back("readdir:"+dir);
    event_loop::Deferred<std::vector<cleanup::Dirent>> output(loop);
    const auto failures=item.value("readdirError",Json::object());
    if(failures.contains(dir))output.reject(std::make_exception_ptr(
      std::runtime_error("readdir "+dir)));
    else if(!files.contains(dir))output.reject(std::make_exception_ptr(
      std::system_error(std::make_error_code(std::errc::no_such_file_or_directory))));
    else{
      std::vector<cleanup::Dirent> values;
      for(const auto& entry:files[dir])values.push_back({entry.at("name").get<std::string>(),
        entry.value("file",false),entry.value("directory",false)});
      output.resolve(std::move(values));
    }
    return output;
  };
  services.fs.unlink=[&](const std::string& path){
    logs.push_back("unlink:"+path);
    event_loop::Deferred<event_loop::Unit> output(loop);
    if(contains(item.value("unlinkError",Json::array()),path))
      output.reject(std::make_exception_ptr(std::runtime_error("unlink "+path)));
    else output.resolve(event_loop::Unit{});
    return output;
  };
  services.fs.rmdir=[&](const std::string& path){
    logs.push_back("rmdir:"+path);
    event_loop::Deferred<event_loop::Unit> output(loop);
    if(contains(item.value("rmdirError",Json::array()),path))
      output.reject(std::make_exception_ptr(std::runtime_error("rmdir "+path)));
    else output.resolve(event_loop::Unit{});
    return output;
  };
  auto pending=cleanup::cleanupOldMessageFiles(loop,"/errors","/logs",
    cleanup::cutoffDateMs(*cleanup::indigenousLogDateMs("2026-10-28T12:00:00.000Z"),30),services);
  std::optional<cleanup::Result> value;
  pending.then([&](const cleanup::Result* result,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);value=*result;
  });
  loop.run();
  if(!value)throw std::runtime_error("cleanup did not complete");
  return Json{{"name",item.at("name")},
    {"value",{{"messages",value->messages},{"errors",value->errors}}},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string input(std::istreambuf_iterator<char>{file},{});
  const auto cases=value::parse(input);Json out=Json::array();
  for(const auto& item:cases)out.push_back(evaluate(item));
  std::cout<<value::stringify(out)<<'\n';
}
