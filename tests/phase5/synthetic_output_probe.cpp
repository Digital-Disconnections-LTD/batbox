#include "gadgets/synthetic_output/SyntheticOutput.hpp"
#include <iostream>

using namespace batbox;
int main(){
  auto base=gadgets::synthetic_output::makeSyntheticOutputGadget(
    "StructuredOutput");
  auto hook=gadgets::synthetic_output::makeHookStructuredOutputGadget(base);
  gadgets::PoetryContext poetryContext;
  auto call=[](gadgets::Gadget& gadget,const Json& input){
    auto result=gadget.call(input,{}, {},Json(),{}).get();
    return Json{{"data",result.data},
      {"structured_output",*result.structuredOutput}};
  };
  bool invalidInput=false,invalidHook=false;
  try{base.parseInput(Json::array());}catch(...){invalidInput=true;}
  try{hook.parseInput(Json{{"reason","missing"}});}catch(...){invalidHook=true;}
  Json row={{"enabled",Json::array({
    gadgets::synthetic_output::isEnabledForNonInteractiveFlight(false),
    gadgets::synthetic_output::isEnabledForNonInteractiveFlight(true)})},
    {"name",base.name},{"isMcp",base.isMcp.value()},
    {"searchHint",*base.searchHint},
    {"maxResultSizeChars",base.maxResultSizeChars},
    {"flags",Json::array({base.isEnabled(),base.isConcurrencySafe({}),
      base.isReadOnly({}),base.isOpenWorld({})})},
    {"description",base.description({},{}).get()},
    {"prompt",base.poetry(poetryContext).get()},
    {"input",Json::array({base.parseInput(Json{{"x",1}}).is_object(),
      !invalidInput})},
    {"output",Json::array({true,false})},
    {"call",call(base,Json{{"ok",true}})},
    {"permission",base.checkPermissions(Json{{"ok",true}},{}).get()},
    {"use",Json::array({base.renderGadgetUseMessage(Json::object(),{}),
      base.renderGadgetUseMessage(Json{{"ok",true},{"reason","why"}},{}),
      base.renderGadgetUseMessage(Json{{"a",1},{"b",2},{"c",3},{"d",4}}, {})})},
    {"rejected",base.renderGadgetUseRejectedMessage({}, {})},
    {"error",base.renderGadgetUseErrorMessage({}, {})},
    {"progress",base.renderGadgetUseProgressMessage({}, {})},
    {"result",base.renderGadgetResultMessage("result",{}, {})},
    {"mapped",base.mapGadgetResultToGadgetResultBlockParam("result","id")},
    {"hookSchema",hook.inputJSONSchema},
    {"hookPrompt",hook.poetry(poetryContext).get()},
    {"hookParse",Json::array({hook.parseInput(Json{{"ok",true},
      {"extra",1}}),!invalidHook})},
    {"hookCall",call(hook,Json{{"ok",false},{"reason","unfinished"}})}};
  std::cout<<row.dump()<<'\n';
}
