#include "attachment_triggers/Attachments.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
namespace {
Json run(const Json& input){
  event_loop::Loop loop;
  attachment_triggers::Services services{loop};
  services.cwd=input.at("cwd").get<std::string>();
  execution::Context context;
  context.dynamicSkillDirTriggers=std::make_shared<runtime::OrderedStringSet>();
  for(const auto& dir:input.at("triggers"))
    context.dynamicSkillDirTriggers->insert(dir.get<std::string>());
  auto pending=attachment_triggers::getDynamicSkillAttachments(context,services);
  Json attachments=nullptr;
  std::exception_ptr failure;
  pending.then([&](const Json* value,const std::exception_ptr& error){
    failure=error;if(value)attachments=*value;
  });
  for(int i=0;i<2000&&attachments.is_null()&&!failure;++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(failure)std::rethrow_exception(failure);
  if(attachments.is_null())throw std::runtime_error("Dynamic skill scan did not settle");
  Json remaining=Json::array();
  for(const auto& dir:*context.dynamicSkillDirTriggers)remaining.push_back(dir);
  return {{"attachments",attachments},{"remaining",remaining}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
