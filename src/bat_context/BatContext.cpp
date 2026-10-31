#include "bat_context/BatContext.hpp"
#include <stdexcept>

namespace batbox::bat_context {
runtime::AsyncLocalStorage<BatContext>& batContextStorage(){
  static runtime::AsyncLocalStorage<BatContext> storage;
  return storage;
}
BatRef getBatContext(){return batContextStorage().getStore();}
bool isSubagentContext(const BatRef& context){
  return context&&context->batType=="subagent";
}
bool isPartnerBatContext(const BatRef& context,
    const std::function<bool()>& isBatSwarmsEnabled){
  if(!isBatSwarmsEnabled)
    throw std::invalid_argument("Agent swarm feature gate required");
  if(isBatSwarmsEnabled())return context&&context->batType=="teammate";
  return false;
}
}
