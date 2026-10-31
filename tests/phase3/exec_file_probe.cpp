#include "process/ExecFileNoThrow.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace batbox;
using namespace batbox::process;
namespace {
std::optional<ExecOptions> parseOptions(const Json& item){
  if(!item.contains("options"))return std::nullopt;
  ExecOptions out;const auto& value=item["options"];
  if(value.contains("timeout"))out.timeout=value["timeout"].get<double>();
  if(value.contains("preserveOutputOnError"))out.preserveOutputOnError=value["preserveOutputOnError"].get<bool>();
  if(value.contains("useCwd"))out.useCwd=value["useCwd"].get<bool>();
  if(value.contains("maxBuffer"))out.maxBuffer=value["maxBuffer"].get<double>();
  if(value.contains("cwd"))out.cwd=value["cwd"].get<std::string>();
  if(value.contains("stdin"))out.stdin=value["stdin"].get<std::string>();
  if(value.contains("input"))out.input=value["input"].get<std::string>();
  if(value.contains("env"))out.env=value["env"].get<std::map<std::string,std::string>>();
  if(value.contains("shell"))out.shell=value["shell"];
  return out;
}
Json requestJson(const ExecaRequest& request){
  Json options=Json::object();const auto& o=request.options;
  if(o.timeout)options["timeout"]=*o.timeout;
  if(o.preserveOutputOnError){} // Wrapper-only option, not passed to execa.
  if(o.maxBuffer)options["maxBuffer"]=*o.maxBuffer;
  if(o.cwd)options["cwd"]=*o.cwd;
  if(o.env)options["env"]=*o.env;
  if(o.shell)options["shell"]=*o.shell;
  if(o.stdin)options["stdin"]=*o.stdin;
  if(o.input)options["input"]=*o.input;
  options["reject"]=false;
  return {{"file",request.file},{"args",request.args},{"options",options}};
}
ExecaResult resultFromJson(const Json& value){
  ExecaResult result;
  result.failed=value.value("failed",false);
  if(value.contains("exitCode"))result.exitCode=value["exitCode"].get<int>();
  result.stdoutText=value.value("stdout",std::string());
  result.stderrText=value.value("stderr",std::string());
  if(value.contains("shortMessage"))result.shortMessage=value["shortMessage"].get<std::string>();
  if(value.contains("signal"))result.signal=value["signal"].get<std::string>();
  return result;
}
Json evaluate(const Json& item){
  event_loop::Loop loop;Json events=Json::array(),record=Json::object();
  ExecImports imports;
  imports.getCwd=[&]{events.push_back("getCwd");
    if(item.value("getCwdThrow",false))throw std::runtime_error("cwd failed");
    return std::string("/work");};
  imports.logError=[&](std::exception_ptr){events.push_back("logError");};
  imports.execa=[&](ExecaRequest request){
    events.push_back("execa");record["request"]=requestJson(request);
    if(item.value("execaThrow",false))throw std::runtime_error("execa threw");
    event_loop::Deferred<ExecaResult> output(loop);
    if(item.value("execaReject",false))
      output.reject(std::make_exception_ptr(std::runtime_error("execa rejected")));
    else output.resolve(resultFromJson(item.at("result")));
    return output;
  };
  try{
    auto pending=item.at("function")=="wrapper"?
      execFileNoThrow(loop,"tool",{"--arg","value"},parseOptions(item),imports):
      execFileNoThrowWithCwd(loop,"tool",{"--arg","value"},parseOptions(item),imports);
    events.push_back("after-call");
    loop.queueMicrotask([&]{events.push_back("sibling");});
    pending.then([&](const ExecResult* value,const std::exception_ptr& error){
      if(error){events.push_back("outer-error");try{std::rethrow_exception(error);}
        catch(const std::exception& e){record["error"]=e.what();}}
      else {events.push_back("outer");record["result"]=value->toJson();}
    });
    loop.run();
  }catch(const std::exception& error){record["syncError"]=error.what();}
  record["events"]=events;return record;
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream file(argv[1]);
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>{file},{}));
  Json output=Json::array();for(const auto& item:cases)output.push_back(evaluate(item));
  std::cout<<value::stringify(output)<<'\n';
}
