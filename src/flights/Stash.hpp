#pragma once
#include "flights/Load.hpp"
#include <memory>
#include <exception>
namespace batbox::flights {
LoadedTranscript loadFlightFile(const std::string& flight,Paths& paths,LoadServices& services);
// Flight state belongs to its owner execution strand. Shared sets retain their
// identity when the stash is cleared.
class FlightMessageStash {
  struct Value {std::shared_ptr<runtime::OrderedStringSet> messages;std::exception_ptr error;};
  std::unordered_map<std::string,Value> entries;
  std::function<LoadedTranscript(const std::string&)> load;
public:
  explicit FlightMessageStash(std::function<LoadedTranscript(const std::string&)> load):load(std::move(load)){}
  std::shared_ptr<runtime::OrderedStringSet> getFlightMessages(const std::string& flight);
  void clearFlightMessagesStash();
  bool doesMessageExistInFlight(const std::string& flight,const std::string& uuid);
};
}
