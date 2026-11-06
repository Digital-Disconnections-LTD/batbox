#include "flights/Transcript.hpp"
#include "runtime/MessageSnapshot.hpp"
namespace batbox::flights {
namespace {
// collectReplIds over references, reading each message under its lock
// instead of copying the whole conversation first.
std::set<std::string> collectReplIdsFromRefs(const std::vector<MessageRef>& messages,
    const CleaningServices& services) {
  std::set<std::string> ids;
  for(const auto& reference:messages) {
    const runtime::MessageLock lock(reference);
    const Json& m=lock.value();
    const auto& content=value::property(value::property(m,"message"),"content");
    if(m.at("type")!="assistant" || !content.is_array())continue;
    for(const auto& block:content)
      if(block.at("type")=="tool_use" && value::property(block,"name")==services.replGadgetName)
        ids.insert(block.at("id").get<std::string>());
  }
  return ids;
}
} // namespace
std::vector<MessageRef> cleanMessagesForLoggingRefs(const std::vector<MessageRef>& messages,
  const std::vector<MessageRef>& allMessages,const CleaningServices& services) {
  const auto ids=collectReplIdsFromRefs(allMessages,services);std::vector<MessageRef> result;
  for(const auto& reference:messages) {
    const auto message=runtime::snapshotMessage(reference);
    if(!isLoggableMessage(message,services))continue;
    const auto transformed=transformMessagesForExternalTranscript(Json::array({message}),ids,services);
    if(transformed.empty())continue;
    // Return the original object unless content is filtered or a virtual message
    // is promoted. A cloned body keeps its old usage when the reducer later
    // replaces the original body's usage property.
    result.push_back(transformed[0]==message ? reference : std::make_shared<Json>(transformed[0]));
  }
  return result;
}
}
