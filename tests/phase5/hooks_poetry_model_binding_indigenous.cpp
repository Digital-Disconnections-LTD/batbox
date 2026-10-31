#include "instructions/hooks/runtime/PoetryModelBinding.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hooks=batbox::instructions::hooks::runtime;

int main(){
  event_loop::Loop loop;
  hooks::PoetryHookServices services;
  model::SimpleQueryRequest observed;
  event_loop::Deferred<model::MessageRef> pending(loop);
  hooks::bindPoetryModelBackend(loop,services,
    [&](model::SimpleQueryRequest request){
      observed=std::move(request);
      return pending;
    });
  hooks::PoetryQueryRequest call;
  call.messages=Json::array({Json{{"type","user"}}});
  call.systemPoetry=Json::array({"system"});
  call.thinkingConfig={{"type","disabled"}};
  call.gadgets=Json::array({Json{{"name","Read"}}});
  call.options={{"model","selected"},{"outputFormat",{{"type","json_schema"}}}};
  call.getGadgetPermissionContext=[&]{
    event_loop::Deferred<Json> output(loop);
    output.resolve(Json{{"mode","default"}});return output;
  };
  Json response;
  bool completed=false;
  services.queryModelWithoutStreaming(std::move(call)).then(
    [&](const Json* value,const std::exception_ptr& failure){
      assert(!failure&&value);
      response=*value;completed=true;
    });
  assert(observed.input.messages.size()==1);
  assert(observed.input.systemPoetry==Json::array({"system"}));
  assert(observed.input.gadgets.at(0).at("name")=="Read");
  assert(observed.input.options.at("model")=="selected");
  assert(observed.input.options.at("querySource")=="hook_prompt");
  assert(observed.input.options.at("isNonInteractiveSession")==true);
  assert(observed.input.options.at("hasAppendSystemPrompt")==false);
  assert(observed.input.options.at("agents")==Json::array());
  assert(observed.input.options.at("mcpTools")==Json::array());
  assert(observed.undefinedOptionKeys.count("toolChoice")==1);
  assert(observed.undefinedOptionKeys.count("agentId")==1);
  assert(observed.ownOptionKeys.size()==10);
  assert(observed.getGadgetPermissionContext);
  Json permission;
  observed.getGadgetPermissionContext().then(
    [&](const Json* value,const std::exception_ptr& failure){
      assert(!failure&&value);permission=*value;});
  auto assistant=std::make_shared<Json>(Json{{"type","assistant"},
    {"message",{{"content",Json::array({Json{{"type","text"},{"text","ok"}}})}}}});
  pending.resolve(assistant);
  assert(!completed);
  loop.runReady();
  assert(permission.at("mode")=="default");
  assert(completed&&response.at("message")==assistant->at("message"));

  hooks::PoetryQueryRequest missing;
  missing.options={{"model","selected"},{"outputFormat",Json::object()}};
  bool rejected=false;
  services.queryModelWithoutStreaming(std::move(missing)).then(
    [&](const Json*,const std::exception_ptr& failure){
      rejected=static_cast<bool>(failure);});
  loop.runReady();
  assert(rejected);
  std::cout<<"prompt model binding preserves request options, permission await and assistant result\n";
}
