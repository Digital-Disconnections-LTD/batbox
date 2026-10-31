#include "messages/TextContent.hpp"
#include "runtime/ArrayJoin.hpp"
#include <stdexcept>

namespace batbox::messages {
std::string extractTextContent(const Json& blocks,const std::string& separator){
  if(!blocks.is_array())throw std::invalid_argument("Text content requires an array");
  Json parts=Json::array();
  for(const auto& block:blocks){
    if(block.is_null())throw std::invalid_argument("Text content block requires a value");
    if(value::property(block,"type")!="text")continue;
    parts.push_back(value::property(block,"text"));
  }
  return value::arrayJoin(parts,separator);
}
std::optional<std::string> getContentText(const Json& content){
  if(content.is_string())return content.get<std::string>();
  if(content.is_array()){
    auto text=value::trim(extractTextContent(content,"\n"));
    if(!text.empty())return text;
  }
  return std::nullopt;
}
}
