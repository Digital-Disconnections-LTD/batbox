#include "git/Stash.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json rows=Json::array();
  for(const auto& item:value::parse(data)){
    event_loop::Loop loop;Json logs=Json::array();git::StashImports imports;
    imports.nowIso=[]{return "2026-10-28T00:00:00.000Z";};
    imports.getFileStatus=[&]{logs.push_back("getFileStatus");
      event_loop::Deferred<git::GitFileStatus> future(loop);
      if(item.value("statusReject",false))
        future.reject(std::make_exception_ptr(std::runtime_error("status failed")));
      else{git::GitFileStatus value;
        value.untracked=item.at("untracked").get<std::vector<std::string>>();
        future.resolve(value);}
      return future;};
    imports.commands.gitExe=[&]{logs.push_back("gitExe");return "/usr/bin/git";};
    imports.commands.execFileNoThrow=[&](const std::string& exe,
      const std::vector<std::string>& args,const process::ExecOptions& options){
      logs.push_back({{"exe",exe},{"args",args},
        {"options",{{"preserveOutputOnError",options.preserveOutputOnError.value()}}}});
      process::ExecResult value;
      value.code=item.at(args[0]=="add"?"addCode":"stashCode").get<int>();
      event_loop::Deferred<process::ExecResult> future(loop);future.resolve(value);return future;
    };
    auto future=git::stashToCleanState(loop,imports,item.at("message").get<std::string>());
    std::optional<bool> result;
    future.then([&](const bool* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);result=*value;});
    loop.runReady();if(!result)throw std::runtime_error("stash pending");
    rows.push_back({{"name",item.at("name")},{"value",*result},{"logs",logs}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
