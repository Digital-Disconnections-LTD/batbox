#include "instructions/hooks/InstructionsLoaded.hpp"
#include <cassert>
#include <stdexcept>

using namespace batbox;
namespace hooks=batbox::instructions::hooks;
namespace project=batbox::instructions::project;

int main(){
  event_loop::Loop loop;
  Json captured=Json::array();
  hooks::InstructionsLoadedImports imports;
  imports.getHooksConfigFromSnapshot=[]()->std::optional<Json>{
    return Json{{"InstructionsLoaded",Json::array({Json::object()})}};};
  imports.getRegisteredHooks=[]()->std::optional<Json>{
    throw std::runtime_error("registered hooks must not be read");};
  imports.createBaseHookInput=[] {return Json{{"session_id","local"}};};
  imports.defaultTimeoutMs=75;
  imports.executeHooksOutsideRepl=[&](hooks::HookDispatch request){
    captured.push_back({{"input",request.hookInput},
      {"undefined",request.ownUndefinedKeys},{"timeout",request.timeoutMs},
      {"query",request.matchQuery}});
    event_loop::Deferred<Json> result(loop);
    result.resolve(Json::object());
    return result;};
  project::DiscoveryServices discovery;
  hooks::bindInstructionsLoaded(loop,discovery,imports);
  assert(discovery.hasInstructionsLoadedHook());
  project::MemoryFileInfo info;
  info.path="/work/RULES.md";info.type="Project";
  info.globs=std::vector<std::string>{"src/**"};
  info.parent="/work/INSTRUCTIONS.md";
  discovery.instructionsLoadedHook(info,"include");
  loop.runReady();
  assert(captured.size()==1);
  const auto& call=captured.at(0);
  assert(call.at("input").at("file_path")==info.path);
  assert(call.at("input").at("load_reason")=="include");
  assert(call.at("input").at("parent_file_path")==*info.parent);
  assert(call.at("input").at("globs")==*info.globs);
  assert(call.at("undefined")==Json::array({"trigger_file_path"}));
  assert(call.at("timeout")==75);
  assert(call.at("query")=="include");
}
