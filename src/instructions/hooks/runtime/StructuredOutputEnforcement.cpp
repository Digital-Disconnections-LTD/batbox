#include "instructions/hooks/runtime/StructuredOutputEnforcement.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
const Json& field(const Json& value,const char* name){
  return value::property(value,name);
}
bool sameName(const Json& block,const std::string& name){
  return field(block,"type")=="tool_use"&&field(block,"name")==name;
}
}
bool hasSuccessfulGadgetCall(const Json& messages,const std::string& gadgetName){
  if(!messages.is_array())return false;
  Json mostRecentGadgetUseId;
  for(auto it=messages.rbegin();it!=messages.rend();++it){
    const auto& message=*it;
    if(message.is_null())continue;
    const auto& content=field(field(message,"message"),"content");
    if(field(message,"type")=="assistant"&&content.is_array()){
      bool found=false;
      for(const auto& block:content)if(sameName(block,gadgetName)){
        mostRecentGadgetUseId=field(block,"id");found=true;break;
      }
      if(found)break;
    }
  }
  if(!value::truthy(mostRecentGadgetUseId))return false;
  for(auto it=messages.rbegin();it!=messages.rend();++it){
    const auto& message=*it;
    if(message.is_null())continue;
    const auto& content=field(field(message,"message"),"content");
    if(field(message,"type")=="user"&&content.is_array()){
      for(const auto& block:content)
        if(field(block,"type")=="tool_result"&&
          field(block,"tool_use_id")==mostRecentGadgetUseId)
          return field(block,"is_error")!=true;
    }
  }
  return false;
}
void registerStructuredOutputEnforcement(
  const std::string& flightId,const std::string& syntheticOutputGadgetName,
  const AddStructuredOutputFunctionHook& addFunctionHook){
  if(!addFunctionHook)
    throw std::invalid_argument("Session function-hook registration required");
  if(syntheticOutputGadgetName.empty())
    throw std::invalid_argument("Structured-output protocol name required");
  addFunctionHook(flightId,"Stop","",
    [syntheticOutputGadgetName](const Json& messages){
      return hasSuccessfulGadgetCall(messages,syntheticOutputGadgetName);
    },"You MUST call the "+syntheticOutputGadgetName+
      " tool to complete this request. Call this tool now.",5000);
}
} // namespace batbox::instructions::hooks::runtime
