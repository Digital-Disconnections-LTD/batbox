#include "bootstrap/RequestState.hpp"
#include <utility>

namespace batbox::bootstrap {
void setLastAPIRequest(RequestState& state,std::shared_ptr<Json> params){
  state.lastAPIRequest=std::move(params);
}
std::shared_ptr<Json> getLastAPIRequest(const RequestState& state){
  return state.lastAPIRequest;
}
void setLastAPIRequestMessages(RequestState& state,
  std::shared_ptr<Json> messages){
  state.lastAPIRequestMessages=std::move(messages);
}
std::shared_ptr<Json> getLastAPIRequestMessages(const RequestState& state){
  return state.lastAPIRequestMessages;
}
void setLastClassifierRequests(RequestState& state,
  std::shared_ptr<Json> requests){
  state.lastClassifierRequests=std::move(requests);
}
std::shared_ptr<Json> getLastClassifierRequests(const RequestState& state){
  return state.lastClassifierRequests;
}
void setStashedBatboxMdContent(RequestState& state,
  std::optional<std::string> content){
  state.stashedBatboxMdContent=std::move(content);
}
const std::optional<std::string>& getStashedBatboxMdContent(
  const RequestState& state){
  return state.stashedBatboxMdContent;
}
void addToInMemoryErrorLog(RequestState& state,ErrorLogEntry errorInfo){
  constexpr std::size_t maxInMemoryErrors=100;
  if(state.inMemoryErrorLog.size()>=maxInMemoryErrors)
    state.inMemoryErrorLog.erase(state.inMemoryErrorLog.begin());
  state.inMemoryErrorLog.push_back(std::move(errorInfo));
}
}
