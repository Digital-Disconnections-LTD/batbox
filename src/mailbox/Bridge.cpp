#include "mailbox/Bridge.hpp"
#include <stdexcept>

namespace batbox::mailbox {
void runMailboxBridgeEffect(Mailbox& mailbox,bool isLoading,
    const SubmitMessage& onSubmitMessage){
  if(isLoading)return;
  const auto message=mailbox.poll();
  if(message){
    if(!onSubmitMessage)
      throw std::invalid_argument("Mailbox submit callback required");
    (void)onSubmitMessage(message->content);
  }
}
} // namespace batbox::mailbox
