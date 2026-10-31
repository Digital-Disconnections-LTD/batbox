#include "flights/Transcript.hpp"
#include "runtime/MessageSnapshot.hpp"
namespace batbox::flights {
std::vector<MessageRef> cleanMessagesForLoggingRefs(const std::vector<MessageRef>& messages,
  const std::vector<MessageRef>& allMessages,const CleaningServices& services) {
  Json all=Json::array();for(const auto& message:allMessages)all.push_back(runtime::snapshotMessage(message));
  const auto ids=collectReplIds(all,services);std::vector<MessageRef> result;
  for(const auto& reference:messages) {
    const auto message=runtime::snapshotMessage(reference);
    if(!isLoggableMessage(message,services))continue;
    if(services.internalUser){result.push_back(reference);continue;}
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
