#include "application/engine/Defaults.hpp"
#include "gadgets/Gadget.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool condition,const char* message="Gadget refresh check failed"){
  if(!condition)throw std::logic_error(message);
}
gadgets::Gadget makeGadget(const std::string& name,bool mcp=false){
  gadgets::Gadget gadget;
  gadget.name=name;
  gadget.isMcp=mcp?std::optional<bool>(true):std::optional<bool>{};
  return gadget;
}
} // namespace

int main(){
  namespace engine=application::engine;
  const gadgets::Gadget read=makeGadget("Read"),bash=makeGadget("Bash"),
    grep=makeGadget("Grep"),glob=makeGadget("Glob");
  gadgets::Gadget extra;
  std::vector<gadgets::Gadget> catalog;
  catalog.push_back(makeGadget("Agent"));
  catalog.push_back(read);catalog.push_back(bash);
  catalog.push_back(grep);catalog.push_back(glob);
  for(const char* name:{"Edit","Write","WebFetch","WebSearch","TodoWrite",
      "TaskOutput","TaskStop","Skill","LSP"})catalog.push_back(makeGadget(name));
  const gadgets::Gadgets bat={&read,&bash,&grep,&glob};
  gadgets::Gadgets live;
  for(const auto& gadget:catalog)live.push_back(&gadget);

  // A main-thread context adopts the live catalog wholesale.
  execution::Context mainThread;
  mainThread.gadgets=live;
  const auto mainRefresh=engine::refreshGadgetsForContext(mainThread,live);
  check(mainRefresh.has_value(),"main thread must refresh");
  check(*mainRefresh==live,"main thread adopts the catalog");

  // A bat context keeps its resolved pool: the catalog's larger set must not
  // replace it, and unchanged pools report nothing to adopt.
  execution::Context batContext;
  batContext.data=Json{{"agentId","bat-1"}};
  batContext.gadgets=bat;
  const auto batRefresh=engine::refreshGadgetsForContext(batContext,live);
  check(!batRefresh.has_value(),"bat pool must survive catalog refresh");

  // Newly-live MCP gadgets join the bat pool; already-present names do not
  // duplicate.
  const gadgets::Gadget mcpLate=makeGadget("mcp:server:late",true);
  const gadgets::Gadget mcpKnown=makeGadget("mcp:server:known",true);
  gadgets::Gadgets grew=live;
  grew.push_back(&mcpLate);
  const auto merged=engine::refreshGadgetsForContext(batContext,grew);
  check(merged.has_value(),"newly-live MCP gadget must be adopted");
  check(merged->size()==5&&merged->back()==&mcpLate,
    "bat pool grows by the new MCP gadget only");
  gadgets::Gadgets replayed=grew;
  replayed.push_back(&mcpKnown);
  execution::Context batWithMcp=batContext;
  const gadgets::Gadget mcpOwned=makeGadget("mcp:server:known",true);
  batWithMcp.gadgets={&read,&bash,&grep,&glob,&mcpOwned};
  const auto kept=engine::refreshGadgetsForContext(batWithMcp,replayed);
  check(kept.has_value(),"the late server still joins once");
  check(kept->size()==6,"known MCP name is not duplicated");
  std::cout<<"refresh gadgets indigenous checks passed\n";
  return 0;
}
