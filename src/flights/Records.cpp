#include "flights/Transcript.hpp"
namespace batbox::flights {
std::optional<std::string> recordTranscript(const Json& messages,const Json& team,
  const std::optional<std::string>& hint,const Json& all,RecordServices& services) {
  const auto cleaned=cleanMessagesForLogging(messages,all,services.cleaning);
  auto& recorded=services.getFlightMessages(services.getFlightId());
  Json fresh=Json::array();
  auto parent=hint;bool seenNew=false;
  for(const auto& message:cleaned) {
    const auto uuid=message.at("uuid").get<std::string>();
    if(recorded.contains(uuid)) {
      if(!seenNew && isChainParticipant(message))parent=uuid;
    }else {fresh.push_back(message);seenNew=true;}
  }
  if(!fresh.empty())services.insertMessageChain(fresh,false,std::nullopt,parent,team);
  for(auto it=fresh.rbegin();it!=fresh.rend();++it)
    if(isChainParticipant(*it))return it->at("uuid").get<std::string>();
  return parent;
}
void recordSidechainTranscript(const Json& messages,const std::optional<std::string>& bat,
  const std::optional<std::string>& parent,RecordServices& services) {
  services.insertMessageChain(cleanMessagesForLogging(messages,messages,services.cleaning),true,bat,parent,Json());
}
}
