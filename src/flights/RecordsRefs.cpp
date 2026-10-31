#include "flights/Transcript.hpp"
#include "runtime/MessageSnapshot.hpp"
namespace batbox::flights {
std::optional<std::string> recordTranscriptRefs(const std::vector<MessageRef>& messages,const Json& team,
  const std::optional<std::string>& hint,const std::vector<MessageRef>& all,RecordRefServices& services) {
  const auto cleaned=cleanMessagesForLoggingRefs(messages,all,services.cleaning);
  auto& recorded=services.getFlightMessages(services.getFlightId());
  std::vector<MessageRef> fresh;auto parent=hint;bool seenNew=false;
  for(const auto& reference:cleaned) {
    const auto message=runtime::snapshotMessage(reference);const auto uuid=message.at("uuid").get<std::string>();
    if(recorded.contains(uuid)) {
      if(!seenNew && isChainParticipant(message))parent=uuid;
    }else {fresh.push_back(reference);seenNew=true;}
  }
  if(!fresh.empty())services.insertMessageChain(fresh,false,std::nullopt,parent,team);
  for(auto it=fresh.rbegin();it!=fresh.rend();++it) {
    const auto message=runtime::snapshotMessage(*it);
    if(isChainParticipant(message))return message.at("uuid").get<std::string>();
  }
  return parent;
}
void recordSidechainTranscriptRefs(const std::vector<MessageRef>& messages,const std::optional<std::string>& bat,
  const std::optional<std::string>& parent,RecordRefServices& services) {
  services.insertMessageChain(cleanMessagesForLoggingRefs(messages,messages,services.cleaning),true,bat,parent,Json());
}
}
