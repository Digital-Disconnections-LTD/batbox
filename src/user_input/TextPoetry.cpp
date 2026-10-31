#include "user_input/TextPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::user_input {
InputResult processTextPoetry(const Json& input,const Json& images,
  const Json& ids,const Json& attachments,const std::optional<std::string>& uuid,
  const std::optional<std::string>& permissionMode,bool isMeta,
  messages::Services& services){
  if(!images.is_array()||!ids.is_array()||!attachments.is_array())
    throw std::invalid_argument("Input image and attachment collections must be arrays");
  Json args=Json::object();
  if(!images.empty()){
    Json content=Json::array();
    if(input.is_string()){
      if(!value::trim(input.get<std::string>()).empty())
        content.push_back(Json{{"type","text"},{"text",input}});
    }else if(input.is_array())for(const auto& block:input)content.push_back(block);
    else throw std::invalid_argument("Prompt input must be text or content blocks");
    for(const auto& image:images)content.push_back(image);
    args["content"]=std::move(content);
    if(!ids.empty())args["imagePasteIds"]=ids;
  }else args["content"]=input;
  if(uuid)args["uuid"]=*uuid;
  if(permissionMode)args["permissionMode"]=*permissionMode;
  if(isMeta)args["isMeta"]=true;
  InputResult result;
  result.messages.push_back(messages::createUserMessage(args,services));
  for(const auto& attachment:attachments)result.messages.push_back(attachment);
  result.shouldQuery=true;
  return result;
}
} // namespace batbox::user_input
