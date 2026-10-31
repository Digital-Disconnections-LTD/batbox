#include "interactive/message_view/Metadata.hpp"
#include <stdexcept>

namespace batbox::interactive::message_view {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
bool containsText(const Json& message){
  const auto& content=field(field(message,"message"),"content");
  if(!content.is_array())throw std::invalid_argument("Assistant content array required");
  for(const auto& block:content)if(field(block,"type")=="text")return true;
  return false;
}
}
std::optional<MetadataPart> timestampMetadata(const Json& message,bool transcript,
    const MetadataImports& imports){
  const auto& timestamp=field(message,"timestamp");
  if(!transcript||!value::truthy(timestamp)||field(message,"type")!="assistant"||
      !containsText(message))return {};
  if(!imports.formatLocalTime||!imports.stringWidth)
    throw std::invalid_argument("Timestamp locale/width imports required");
  const auto formatted=imports.formatLocalTime(timestamp.get<std::string>());
  return MetadataPart{formatted,imports.stringWidth(formatted)};
}
std::optional<MetadataPart> modelMetadata(const Json& message,bool transcript,
    const MetadataImports& imports){
  const auto& model=field(field(message,"message"),"model");
  if(!transcript||field(message,"type")!="assistant"||!value::truthy(model)||
      !containsText(message))return {};
  if(!imports.stringWidth)throw std::invalid_argument("Model width import required");
  const auto text=model.get<std::string>();
  return MetadataPart{text,imports.stringWidth(text)+8};
}
} // namespace batbox::interactive::message_view
