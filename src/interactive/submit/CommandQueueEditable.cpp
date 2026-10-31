#include "interactive/submit/CommandQueue.hpp"
#include "messages/TextContent.hpp"
#include <stdexcept>

namespace batbox::interactive::submit {
namespace {
std::string textValue(const CommandRef& command){
  const auto& value=value::property(*command,"value");
  if(value.is_string())return value.get<std::string>();
  return messages::extractTextContent(value,"\n");
}
Json imageBlocks(const CommandRef& command,std::int64_t startId){
  Json result=Json::array();
  const auto& value=value::property(*command,"value");
  if(!value.is_array())return result;
  std::size_t index=0;
  for(const auto& block:value){
    if(value::property(block,"type")!="image")continue;
    const auto& source=value::property(block,"source");
    if(value::property(source,"type")!="base64")continue;
    Json image={{"id",startId+static_cast<std::int64_t>(index)},
      {"type","image"},{"content",value::property(source,"data")},
      {"mediaType",value::property(source,"media_type")},
      {"filename","image"+std::to_string(index+1)}};
    result.push_back(std::move(image));++index;
  }
  return result;
}
}
std::optional<PopEditableResult> CommandQueue::popAllEditable(
    const std::string& currentInput,std::size_t currentCursorOffset,
    double nowMs){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  if(queue_.empty())return {};
  CommandList editable,nonEditable;
  for(const auto& command:queue_)
    (isQueuedCommandEditable(command)?editable:nonEditable).push_back(command);
  if(editable.empty())return {};
  std::vector<std::string> queuedTexts;
  std::string queuedJoined;
  for(const auto& command:editable){
    const auto text=textValue(command);
    if(!queuedTexts.empty())queuedJoined+='\n';
    queuedJoined+=text;queuedTexts.push_back(text);
  }
  std::string combined;
  for(const auto& text:queuedTexts){
    if(text.empty())continue;
    if(!combined.empty())combined+='\n';
    combined+=text;
  }
  if(!currentInput.empty()){
    if(!combined.empty())combined+='\n';
    combined+=currentInput;
  }
  PopEditableResult result;
  result.text=std::move(combined);
  result.cursorOffset=value::stringLength(queuedJoined)+1+currentCursorOffset;
  std::int64_t nextImageId=static_cast<std::int64_t>(nowMs);
  for(const auto& command:editable){
    const auto& pasted=value::property(*command,"pastedContents");
    if(pasted.is_object())for(const auto& key:value::objectKeys(pasted)){
      const auto& value=pasted.at(key);
      if(value::property(value,"type")=="image")result.images.push_back(value);
    }
    const auto blockImages=imageBlocks(command,nextImageId);
    for(const auto& image:blockImages)result.images.push_back(image);
    nextImageId+=static_cast<std::int64_t>(blockImages.size());
  }
  for(const auto& command:editable){
    const auto& value=value::property(*command,"value");
    log("popAll",value.is_string()?
      std::optional<std::string>(value.get<std::string>()):std::nullopt);
  }
  queue_=std::move(nonEditable);notify();
  return result;
}
} // namespace batbox::interactive::submit
