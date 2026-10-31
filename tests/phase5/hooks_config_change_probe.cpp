#include "instructions/hooks/runtime/ConfigChangeHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json dispatch=Json::object();bool ownsFilePath=false;
  hook::EnvHookServices services;
  services.defaultTimeoutMs=600000;
  services.createBaseHookInput=[]{return hook::BaseHookInput{"s1","/tmp/t","/work"};};
  services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch value){
    dispatch={{"hookInput",value.hookInput},{"timeoutMs",value.timeoutMs},
      {"matchQuery",value.matchQuery}};
    ownsFilePath=value.hookInput.contains("file_path")||
      std::find(value.ownUndefinedKeys.begin(),value.ownUndefinedKeys.end(),
        "file_path")!=value.ownUndefinedKeys.end();
    std::vector<hook::HookResult> results;
    for(const auto& row:item.at("results"))
      results.emplace_back(row.at("command").get<std::string>(),
        row.at("output").get<std::string>(),row.at("succeeded").get<bool>(),
        row.at("blocked").get<bool>());
    event_loop::Deferred<std::vector<hook::HookResult>> result(loop);
    result.resolve(std::move(results));return result;};
  std::optional<std::string> filePath;
  if(item.contains("filePath"))filePath=item.at("filePath").get<std::string>();
  std::optional<double> timeout;
  if(item.contains("timeout"))timeout=item.at("timeout").get<double>();
  Json result=Json::array();
  hook::executeConfigChangeHooks(loop,services,item.at("source").get<std::string>(),
    filePath,timeout).then([&](const std::vector<hook::HookResult>* values,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    for(const auto& row:*values)result.push_back(row.toJson());});
  loop.run();
  return {{"name",item.at("name")},{"dispatch",dispatch},
    {"ownsFilePath",ownsFilePath},{"result",result}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
