#include "application_hooks/Provider.hpp"
#include <filesystem>
#include <future>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
}
int main(){
  event_loop::Loop loop;
  application_hooks::Inputs inputs;
  inputs.config.cwd=std::filesystem::temp_directory_path();
  inputs.config.stateDirectory=inputs.config.cwd/"batbox-hook-test";
  inputs.flightId="native-hook-test";
  inputs.settings={{"hooks",{{"PreToolUse",Json::array({
    Json{{"matcher","Bash"},{"hooks",Json::array({
      Json{{"type","command"},{"command",
        "printf '{\"hookSpecificOutput\":{\"hookEventName\":\"PreToolUse\",\"permissionDecision\":\"allow\",\"updatedInput\":{\"command\":\"pwd\"}}}'"}}
    })}}
  })}}}};
  inputs.settings["hooks"]["PreCompact"]=Json::array({
    Json{{"matcher","manual"},{"hooks",Json::array({
      Json{{"type","command"},{"command","printf 'Preserve test context'"}}
    })}}});
  auto provider=std::make_shared<application_hooks::Provider>(loop,inputs);
  std::vector<Json> matched,empty;
  application_hooks::hooks::CompactHookOutput compact;
  std::exception_ptr failure;
  auto keep=loop.hold();int done=0;
  provider->run("PreToolUse",{{"tool_name","Bash"},
    {"tool_input",{{"command","echo bad"}}}},"Bash").then(
    [&](const std::vector<Json>* value,const std::exception_ptr& error){
      if(error)failure=error;else matched=*value;
      if(++done==3)loop.stop();});
  provider->run("PostToolUse",{{"tool_name","Bash"}},"Bash").then(
    [&](const std::vector<Json>* value,const std::exception_ptr& error){
      if(error)failure=error;else empty=*value;
      if(++done==3)loop.stop();});
  provider->preCompact("manual",std::nullopt).then(
    [&](const application_hooks::hooks::CompactHookOutput* value,
        const std::exception_ptr& error){
      if(error)failure=error;else compact=*value;
      if(++done==3)loop.stop();});
  loop.run();keep.release();
  if(failure)std::rethrow_exception(failure);
  if(!empty.empty())throw std::logic_error("No matching hooks must be empty");
  if(!compact.newCustomInstructions||
      *compact.newCustomInstructions!="Preserve test context")
    throw std::logic_error("PreCompact command output missing");
  bool allowed=false;
  for(const auto& row:matched)
    if(value::property(row,"permissionBehavior")=="allow"&&
        value::property(value::property(row,"updatedInput"),"command")=="pwd")
      allowed=true;
  if(!allowed)throw std::logic_error("Command hook permission result missing");
  application_hooks::PermissionImports permission;
  int canCalls=0,ruleCalls=0;
  permission.canUseGadget=[&](const gadgets::Gadget&,const Json&,
      const execution::Context&,const Json&,const std::string&,
      const std::optional<Json>& forced){
    ++canCalls;return ready(Json{{"behavior",forced?"ask":"allow"}});};
  permission.checkRuleBasedPermissions=[&](const gadgets::Gadget&,
      const Json&,const execution::Context&){
    ++ruleCalls;return ready(std::optional<Json>{});};
  gadgets::Gadget gadget;gadget.name="Bash";
  execution::Context context;
  const Json approval={{"behavior","allow"},{"updatedInput",
    {{"command","pwd"}}}};
  const auto resolved=application_hooks::resolveHookPermissionDecision(
    approval,gadget,{{"command","echo bad"}},context,Json::object(),
    "id",permission);
  if(value::property(value::property(resolved,"input"),"command")!="pwd"||
      canCalls!=0||ruleCalls!=1)
    throw std::logic_error("Hook allow/rule check precedence changed");
  permission.checkRuleBasedPermissions=[&](const gadgets::Gadget&,
      const Json&,const execution::Context&){
    return ready(std::optional<Json>(Json{{"behavior","deny"},
      {"message","settings deny"}}));};
  const auto denied=application_hooks::resolveHookPermissionDecision(
    approval,gadget,Json::object(),context,Json::object(),"id",permission);
  if(value::property(value::property(denied,"decision"),"behavior")!="deny")
    throw std::logic_error("Settings deny must override hook allow");
  gadget.requiresUserInteraction=[](){return true;};
  const auto interactive=application_hooks::resolveHookPermissionDecision(
    Json{{"behavior","allow"}},gadget,Json::object(),context,
    Json::object(),"id",permission);
  if(value::property(value::property(interactive,"decision"),"behavior")!="allow"||
      canCalls!=1)
    throw std::logic_error("Interaction guard must call permission provider");
  std::cout<<"Native hook matching, command, and permission decision passed\n";
}
