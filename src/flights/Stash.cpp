#include "flights/Stash.hpp"
namespace batbox::flights {
LoadedTranscript loadFlightFile(const std::string& flight,Paths& paths,LoadServices& services) {
  return loadTranscriptFile(paths.getFlightProjectTranscriptPath(flight),false,services);
}
std::shared_ptr<runtime::OrderedStringSet> FlightMessageStash::getFlightMessages(const std::string& flight) {
  if(const auto at=entries.find(flight);at!=entries.end()) {
    if(at->second.error)std::rethrow_exception(at->second.error);
    return at->second.messages;
  }
  try {
    const auto transcript=load(flight);auto result=std::make_shared<runtime::OrderedStringSet>();
    for(const auto& uuid:transcript.messages.keys())result->insert(uuid);
    entries[flight]={result,nullptr};return result;
  }catch(...){entries[flight]={nullptr,std::current_exception()};throw;}
}
void FlightMessageStash::clearFlightMessagesStash(){entries.clear();}
bool FlightMessageStash::doesMessageExistInFlight(const std::string& flight,const std::string& uuid) {
  return getFlightMessages(flight)->contains(uuid);
}
}
