#include "flights/Title.hpp"
#include "runtime/ArrayJoin.hpp"

namespace batbox::flights {
namespace {constexpr std::size_t maxConversationText=1000;}
std::string extractConversationText(const Json& messages){
  Json parts=Json::array();
  for(const auto& message:messages){
    const auto& type=value::property(message,"type");
    if(type!="user"&&type!="assistant")continue;
    if(value::truthy(value::property(message,"isMeta")))continue;
    const auto& origin=value::property(message,"origin");
    if(value::truthy(origin)&&value::property(origin,"kind")!="human")continue;
    const auto& content=value::property(value::property(message,"message"),"content");
    if(content.is_string())parts.push_back(content);
    else if(content.is_array())for(const auto& block:content){
      if(value::property(block,"type")=="text"&&block.is_object()&&block.contains("text"))
        parts.push_back(block.at("text"));
    }
  }
  const auto text=value::arrayJoin(parts,"\n");
  return value::stringLength(text)>maxConversationText?
    value::slice(text,-static_cast<double>(maxConversationText)):text;
}
}
