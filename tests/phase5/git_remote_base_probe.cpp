#include "git/RemoteBase.hpp"
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
    event_loop::Loop loop;Json logs=Json::array();git::CommandImports imports;
    imports.gitExe=[&]{logs.push_back("gitExe");return "/usr/bin/git";};
    imports.execFileNoThrow=[&](const std::string& exe,
      const std::vector<std::string>& args,const process::ExecOptions& options){
      logs.push_back({{"exe",exe},{"args",args},
        {"options",{{"preserveOutputOnError",options.preserveOutputOnError.value()}}}});
      Json answer;
      if(args[0]=="rev-parse"&&args[1]=="--abbrev-ref")answer=item.at("tracking");
      else if(args[0]=="remote")answer=item.at("remote");
      else answer={{"code",item.at("candidates").value(args[2],1)},{"stdout",""}};
      process::ExecResult value;
      value.code=answer.at("code").get<int>();value.stdoutText=answer.at("stdout").get<std::string>();
      event_loop::Deferred<process::ExecResult> pending(loop);pending.resolve(value);return pending;
    };
    auto pending=git::findRemoteBase(loop,imports);
    std::optional<std::optional<std::string>> value;
    pending.then([&](const std::optional<std::string>* result,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);value=*result;});
    loop.runReady();if(!value)throw std::runtime_error("remote base pending");
    rows.push_back({{"name",item.at("name")},
      {"value",*value?Json(**value):Json(nullptr)},{"logs",logs}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
