#include "instructions/hooks/runtime/BatSyntheticGadgetBinding.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
namespace hooks=batbox::instructions::hooks::runtime;
int main(){
  hooks::BatHookServices bat;
  hooks::bindBatSyntheticOutputGadget(bat,"StructuredOutput");
  auto specialized=bat.createStructuredOutputGadget();
  assert(specialized.shape.at("name")=="StructuredOutput");
  assert(specialized.indigenous);
  assert(specialized.indigenous->poetry({}).get()==
    "Use this tool to return your verification result. You MUST call this tool exactly once at the end of your response.");
  const Json parsed=specialized.indigenous->parseInput(
    Json{{"ok",true},{"extra",1}});
  assert(parsed==Json({{"ok",true}}));
  gadgets::Gadget read;read.name="Read";
  gadgets::Gadget duplicate;duplicate.name="OldTool";
  duplicate.aliases={"StructuredOutput"};
  gadgets::Gadget disallowed;disallowed.name="AgentTool";
  execution::Context parent;
  parent.gadgets={&read,&duplicate,&disallowed};
  parent.data={{"options",{{"mainLoopModel","parent"},
    {"unrelated","retained"}}},{"parentValue",true}};
  std::stop_source stop;
  hooks::BatQueryRequest request;
  request.batId="hook-agent-1";request.model="selected";
  request.signal=stop.get_token();
  request.indigenousStructuredOutputGadget=specialized.indigenous;
  auto context=hooks::makeBatHookQueryContext(request,std::move(parent),
    [](const std::string& name){return name=="AgentTool";},
    "StructuredOutput");
  assert(context.gadgets.size()==2);
  assert(context.gadgets.at(0)==&read);
  assert(context.gadgets.at(1)==specialized.indigenous.get());
  assert(context.stop==stop.get_token());
  assert(context.data.at("agentId")=="hook-agent-1");
  const auto& options=context.data.at("options");
  assert(options.at("mainLoopModel")=="selected");
  assert(options.at("thinkingConfig").at("type")=="disabled");
  assert(options.at("isNonInteractiveSession")==true);
  assert(options.at("unrelated")=="retained");
  assert(context.data.at("parentValue")==true);
  std::cout<<"Bat Gadget context retains the selected Indigenous Gadget and its options\n";
}
