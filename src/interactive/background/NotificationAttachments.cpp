#include "interactive/background/NotificationAttachments.hpp"
#include "query/OwnerPromiseAll.hpp"
#include <stdexcept>

namespace batbox::interactive::background {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
bool validImage(const Json& value){
  if(value.is_null())throw std::invalid_argument("Null pasted content");
  const auto& content=field(value,"content");
  return field(value,"type")=="image"&&content.is_string()&&
    !content.get<std::string>().empty();
}
std::vector<Json> imageContents(const Json& pasted){
  std::vector<Json> result;
  if(pasted.is_array()){
    for(const auto& value:pasted)if(validImage(value))result.push_back(value);
    return result;
  }
  if(!pasted.is_object())return result;
  for(const auto& key:value::objectKeys(pasted)){
    const auto& value=pasted.at(key);
    if(validImage(value))result.push_back(value);
  }
  return result;
}
Json imagePasteIds(const Json& pasted){
  Json result=Json::array();
  for(const auto& image:imageContents(pasted))result.push_back(field(image,"id"));
  return result;
}
std::string extractText(const Json& blocks){
  if(!blocks.is_array())throw std::invalid_argument("Content blocks array required");
  std::string result;
  bool first=true;
  for(const auto& block:blocks){
    if(block.is_null())throw std::invalid_argument("Null content block");
    if(field(block,"type")!="text")continue;
    if(!first)result+='\n';
    first=false;
    const auto& text=field(block,"text");
    if(text.is_string())result+=text.get<std::string>();
  }
  return result;
}
event_loop::Deferred<Json> buildImageBlocks(event_loop::Loop& owner,
    const Json& pasted,const AttachmentServices& services){
  event_loop::Deferred<Json> result(owner);
  const auto images=imageContents(pasted);
  if(images.empty()){result.resolve(Json::array());return result;}
  if(!services.maybeResizeAndDownsampleImageBlock)
    throw std::invalid_argument("Image resize binding required");
  std::vector<event_loop::Deferred<Json>> pending;
  for(const auto& image:images){
    const auto& media=field(image,"mediaType");
    Json block={{"type","image"},{"source",{
      {"type","base64"},{"media_type",value::truthy(media)?media:Json("image/png")},
      {"data",field(image,"content")}}}};
    pending.push_back(services.maybeResizeAndDownsampleImageBlock(block));
  }
  query::ownerPromiseAll(owner,pending).then(
    [result](const std::vector<Json>* blocks,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      Json value=Json::array();
      if(blocks)for(const auto& block:*blocks)value.push_back(block);
      result.resolve(std::move(value));
    });
  return result;
}
void copyOptional(Json& target,const Json& source,const char* from,
    const char* to){
  if(source.is_object()&&source.contains(from))target[to]=source.at(from);
}
}
event_loop::Deferred<Json> getQueuedCommandAttachments(event_loop::Loop& owner,
    const submit::CommandList& commands,const AttachmentServices& services){
  if(!owner.isOwnerThread())throw std::logic_error("Attachment owner required");
  std::vector<event_loop::Deferred<Json>> pending;
  for(const auto& command:commands){
    if(!command)continue;
    const auto& mode=field(*command,"mode");
    if(mode!="prompt"&&mode!="task-notification")continue;
    event_loop::Deferred<Json> item(owner);
    pending.push_back(item);
    try{
      const auto input=*command;
      buildImageBlocks(owner,field(input,"pastedContents"),services).then(
        [item,input](const Json* blocks,const std::exception_ptr& error){
          if(error){item.reject(error);return;}
          try{
            Json poetry=field(input,"value");
            if(blocks&&blocks->is_array()&&!blocks->empty()){
              const auto text=poetry.is_string()?poetry.get<std::string>():
                extractText(poetry);
              Json withImages=Json::array();
              withImages.push_back(Json{{"type","text"},{"text",text}});
              for(const auto& block:*blocks)withImages.push_back(block);
              poetry=std::move(withImages);
            }
            Json attachment={{"type","queued_command"},{"prompt",poetry}};
            copyOptional(attachment,input,"uuid","source_uuid");
            const auto ids=imagePasteIds(field(input,"pastedContents"));
            if(!ids.empty())attachment["imagePasteIds"]=ids;
            copyOptional(attachment,input,"mode","commandMode");
            copyOptional(attachment,input,"origin","origin");
            copyOptional(attachment,input,"isMeta","isMeta");
            item.resolve(std::move(attachment));
          }catch(...){item.reject(std::current_exception());}
        });
    }catch(...){item.reject(std::current_exception());}
  }
  event_loop::Deferred<Json> result(owner);
  query::ownerPromiseAll(owner,pending).then(
    [result](const std::vector<Json>* values,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      Json array=Json::array();
      if(values)for(const auto& item:*values)array.push_back(item);
      result.resolve(std::move(array));
    });
  return result;
}
TurnMessageRef createAttachmentMessage(const Json& attachment,
    const std::function<std::string()>& randomUuid,
    const std::function<std::string()>& nowIso){
  if(!randomUuid||!nowIso)
    throw std::invalid_argument("Attachment UUID and time bindings required");
  return std::make_shared<Json>(Json{{"attachment",attachment},
    {"type","attachment"},{"uuid",randomUuid()},
    {"timestamp",nowIso()}});
}
} // namespace batbox::interactive::background
