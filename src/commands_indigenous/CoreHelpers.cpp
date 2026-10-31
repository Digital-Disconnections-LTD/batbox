#include "commands_indigenous/CoreHelpers.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::commands_indigenous {
std::string compactDisplayText(bool verbose,const std::string& upgradeTip,
    const std::string& expandShortcut,
    const std::optional<std::string>& userDisplayMessage,
    const std::function<std::string(const std::string&)>& dim){
  std::vector<std::string> parts;
  if(!verbose)parts.push_back("("+expandShortcut+" to see full summary)");
  if(userDisplayMessage&&!userDisplayMessage->empty())
    parts.push_back(*userDisplayMessage);
  if(!upgradeTip.empty())parts.push_back(upgradeTip);
  std::string result="Compacted ";
  for(std::size_t i=0;i<parts.size();++i){
    if(i)result+='\n';
    result+=parts[i];
  }
  return dim?dim(result):result;
}

Json filterResumableFlights(const Json& logs,
    const std::string& currentFlightId,
    const std::function<std::string(const Json&)>& flightIdFromLog){
  if(!logs.is_array()||!flightIdFromLog)
    throw std::invalid_argument("Conversation logs and Flight identity required");
  Json result=Json::array();
  for(const auto& log:logs){
    if(value::truthy(value::property(log,"isSidechain")))continue;
    if(flightIdFromLog(log)==currentFlightId)continue;
    result.push_back(log);
  }
  return result;
}
} // namespace batbox::commands_indigenous
