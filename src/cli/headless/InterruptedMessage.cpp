#include "cli/headless/InterruptedMessage.hpp"
#include <algorithm>

namespace batbox::cli::headless {
void removeInterruptedMessage(std::vector<Json>& messages,
                              const Json& interruptedUserMessage){
  const auto& uuid=value::property(interruptedUserMessage,"uuid");
  const auto at=std::find_if(messages.begin(),messages.end(),
    [&](const Json& message){return value::property(message,"uuid")==uuid;});
  if(at!=messages.end()){
    const auto remaining=static_cast<std::size_t>(messages.end()-at);
    messages.erase(at,at+std::min<std::size_t>(2,remaining));
  }
}
}
