#include "instructions/hooks/runtime/BatSyntheticGadgetBinding.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
void bindBatSyntheticOutputGadget(BatHookServices& bat,
  std::string syntheticOutputGadgetName){
  if(syntheticOutputGadgetName.empty())
    throw std::invalid_argument("Structured-output protocol name required");
  const auto base=std::make_shared<gadgets::Gadget>(
    gadgets::synthetic_output::makeSyntheticOutputGadget(
      syntheticOutputGadgetName));
  bat.structuredOutputGadgetName=syntheticOutputGadgetName;
  bat.createStructuredOutputGadget=[base]{
    auto gadget=std::make_shared<gadgets::Gadget>(
      gadgets::synthetic_output::makeHookStructuredOutputGadget(*base));
    Json shape={{"name",gadget->name},{"isMcp",false},
      {"searchHint",*gadget->searchHint},
      {"maxResultSizeChars",gadget->maxResultSizeChars},
      {"inputJSONSchema",gadget->inputJSONSchema}};
    return BatStructuredOutputGadget{std::move(shape),std::move(gadget)};
  };
}
execution::Context makeBatHookQueryContext(
  const BatQueryRequest& request,execution::Context parent,
  const std::function<bool(const std::string&)>& isBatDisallowedGadget,
  const std::string& syntheticOutputGadgetName){
  if(!isBatDisallowedGadget)
    throw std::invalid_argument("Agent disallowed-tool set required");
  if(!request.indigenousStructuredOutputGadget)
    throw std::invalid_argument("Native structured-output Tool required");
  if(request.indigenousStructuredOutputGadget->name!=syntheticOutputGadgetName)
    throw std::invalid_argument("Structured-output protocol name mismatch");
  gadgets::Gadgets filtered;
  filtered.reserve(parent.gadgets.size()+1);
  for(const auto* gadget:parent.gadgets){
    if(!gadget)continue;
    if(gadgets::gadgetMatchesName(*gadget,syntheticOutputGadgetName))continue;
    if(isBatDisallowedGadget(gadget->name))continue;
    filtered.push_back(gadget);
  }
  filtered.push_back(request.indigenousStructuredOutputGadget.get());
  parent.gadgets=std::move(filtered);
  parent.stop=request.signal;
  parent.data["agentId"]=request.batId;
  auto& options=parent.data["options"];
  if(!options.is_object())options=Json::object();
  options["mainLoopModel"]=request.model;
  options["isNonInteractiveSession"]=true;
  options["thinkingConfig"]={{"type","disabled"}};
  return parent;
}
} // namespace batbox::instructions::hooks::runtime
