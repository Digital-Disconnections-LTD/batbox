#include "interactive/navigation/Rewind.hpp"
#include <stdexcept>

namespace batbox::interactive::navigation {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Message-edit binding required: ")+name);
}
const Json& field(const Json& value,const char* name){return value::property(value,name);}
}
event_loop::Deferred<event_loop::Unit> Rewind::editMessage(
    const std::string& renderUuid,const TurnMessagesRef& displayed,
    value::ValueRef fileHistory){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Message edit owner required");
  event_loop::Deferred<event_loop::Unit> result(services_.owner);
  try{
    need(displayed,"displayed messages");
    const auto prefix=value::slice(renderUuid,0,24);
    std::size_t index=0;
    while(index<displayed->size()){
      const auto& uuid=field(*(*displayed)[index],"uuid");
      if(!uuid.is_string())throw std::invalid_argument("Message UUID required");
      if(value::slice(uuid.get<std::string>(),0,24)==prefix)break;
      ++index;
    }
    if(index==displayed->size()){
      result.resolve({});return result;
    }
    const auto raw=(*displayed)[index];
    need(services_.selectableUserMessage,"selectableUserMessage");
    if(!services_.selectableUserMessage(raw)){
      result.resolve({});return result;
    }
    need(services_.fileHistoryHasAnyChanges,"fileHistoryHasAnyChanges");
    const auto& uuid=field(*raw,"uuid");
    auto pending=services_.fileHistoryHasAnyChanges(fileHistory,
      uuid.get<std::string>());
    if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("File-history Promise crossed owner loop");
    auto self=shared_from_this();
    pending.then([self,raw,displayed,index,result](const bool* changed,
        const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      try{
        if(!changed)throw std::logic_error("File-history result missing");
        need(self->services_.messagesAfterAreOnlySynthetic,
          "messagesAfterAreOnlySynthetic");
        const bool onlySynthetic=self->services_.messagesAfterAreOnlySynthetic(
          displayed,index);
        if(!*changed&&onlySynthetic){
          need(self->services_.onCancel,"onCancel");
          self->services_.onCancel();
          self->handleRestoreMessage(raw);
        }else{
          self->setPreselect(raw);
          self->setSelectorVisible(true);
        }
        result.resolve({});
      }catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::interactive::navigation
