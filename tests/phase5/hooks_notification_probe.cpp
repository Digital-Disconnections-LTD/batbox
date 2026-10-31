#include "instructions/hooks/runtime/NotificationHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json dispatch=Json::object();bool ownsTitle=false;
  hook::EnvHookServices services;
  services.defaultTimeoutMs=600000;
  services.createBaseHookInput=[]{return hook::BaseHookInput{"s1","/tmp/t","/work"};};
  services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch value){
    dispatch={{"hookInput",value.hookInput},{"timeoutMs",value.timeoutMs},
      {"matchQuery",value.matchQuery}};
    ownsTitle=value.hookInput.contains("title")||
      std::find(value.ownUndefinedKeys.begin(),value.ownUndefinedKeys.end(),
        "title")!=value.ownUndefinedKeys.end();
    event_loop::Deferred<std::vector<hook::HookResult>> output(loop);
    if(item.contains("executionError"))output.reject(std::make_exception_ptr(
      std::runtime_error(item.at("executionError").get<std::string>())));
    else output.resolve({});
    return output;};
  std::optional<std::string> title;
  if(item.contains("title"))title=item.at("title").get<std::string>();
  std::optional<double> timeout;
  if(item.contains("timeout"))timeout=item.at("timeout").get<double>();
  Json error=nullptr;
  hook::executeNotificationHooks(loop,services,item.at("message").get<std::string>(),
    title,item.at("notificationType").get<std::string>(),timeout).then(
    [&](const event_loop::Unit*,const std::exception_ptr& failure){
      if(failure){try{std::rethrow_exception(failure);}catch(const std::exception& e){
        error=e.what();}}
    });
  loop.run();
  return {{"name",item.at("name")},{"dispatch",dispatch},
    {"ownsTitle",ownsTitle},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
