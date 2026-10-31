#include "interactive/navigation/MessageCopy.hpp"
#include <stdexcept>

namespace batbox::interactive::navigation {
void copyMessageText(const std::string& text,const CopyServices& services){
  if(!services.owner.isOwnerThread())
    throw std::logic_error("Clipboard owner required");
  if(!services.setClipboard)
    throw std::invalid_argument("Clipboard binding required");
  const auto pending=services.setClipboard(text);
  if(!services.owner.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Clipboard Promise crossed owner loop");
  pending.then([writer=services.writeTerminal,notify=services.addNotification,
      error=services.onDetachedError](const std::string* raw,
      const std::exception_ptr& failure){
    if(failure){if(error)error(failure);return;}
    if(!raw)throw std::logic_error("Clipboard result missing");
    try{
      if(!raw->empty()){
        if(!writer)throw std::invalid_argument("Terminal writer required");
        writer(*raw);
      }
      if(!notify)throw std::invalid_argument("Copy notification required");
      notify(CopyNotification{});
    }catch(...){
      if(!error)throw;
      error(std::current_exception());
    }
  });
}
} // namespace batbox::interactive::navigation
