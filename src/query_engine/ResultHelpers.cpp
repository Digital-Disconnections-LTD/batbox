#include "query_engine/ResultHelpers.hpp"
#include <algorithm>
namespace batbox::query_engine {
bool isResultSuccessful(const MessageRef& message,const Json& stopReason){
  if(!message)return false;
  const auto& type=value::property(*message,"type");
  const auto& content=value::property(value::property(*message,"message"),"content");
  if(type=="assistant"){
    if(!content.is_array()||content.empty())return false;
    const auto& lastType=value::property(content.back(),"type");
    return lastType=="text"||lastType=="thinking"||lastType=="redacted_thinking";
  }
  if(type=="user"&&content.is_array()&&!content.empty()&&
      std::all_of(content.begin(),content.end(),[](const Json& block){
        return block.contains("type")&&block.at("type")=="tool_result";
      }))return true;
  return stopReason=="end_turn";
}
}
