#include "mailbox/Mailbox.hpp"
#include <stdexcept>

namespace batbox::mailbox {
void Mailbox::requireOwner()const{
  if(!loop_.isOwnerThread())throw std::logic_error("Mailbox owner required");
}
std::size_t Mailbox::length()const{requireOwner();return queue_.size();}
std::size_t Mailbox::revision()const{requireOwner();return revision_;}
bool Mailbox::matches(const Predicate& predicate,const MessageRef& message){
  return !predicate||predicate(message);
}
void Mailbox::notify(){changed_.emit();}
void Mailbox::send(MessageRef message){
  requireOwner();
  ++revision_;
  const auto originalLength=waiters_.size();
  for(std::size_t index=0;index<originalLength;++index){
    if(index>=waiters_.size())
      throw std::runtime_error("Cannot read properties of undefined (reading 'fn')");
    const auto waiter=waiters_[index];
    if(!waiter)
      throw std::runtime_error("Cannot read properties of undefined (reading 'fn')");
    if(matches(waiter->predicate,message)){
      const auto selected=index<waiters_.size()?waiters_[index]:nullptr;
      if(index<waiters_.size())
        waiters_.erase(waiters_.begin()+static_cast<std::ptrdiff_t>(index));
      if(selected){
        selected->result.resolve(std::move(message));
        notify();
        return;
      }
      break;
    }
  }
  queue_.push_back(std::move(message));
  notify();
}
MessageRef Mailbox::poll(Predicate predicate){
  requireOwner();
  const auto originalLength=queue_.size();
  for(std::size_t index=0;index<originalLength;++index){
    const auto candidate=index<queue_.size()?queue_[index]:MessageRef{};
    if(matches(predicate,candidate)){
      const auto message=index<queue_.size()?queue_[index]:MessageRef{};
      if(index<queue_.size())
        queue_.erase(queue_.begin()+static_cast<std::ptrdiff_t>(index));
      return message;
    }
  }
  return {};
}
event_loop::Deferred<MessageRef> Mailbox::receive(Predicate predicate){
  requireOwner();
  const auto originalLength=queue_.size();
  for(std::size_t index=0;index<originalLength;++index){
    const auto candidate=index<queue_.size()?queue_[index]:MessageRef{};
    if(matches(predicate,candidate)){
      const auto message=index<queue_.size()?queue_[index]:MessageRef{};
      if(index<queue_.size())
        queue_.erase(queue_.begin()+static_cast<std::ptrdiff_t>(index));
      if(message){
        notify();
        event_loop::Deferred<MessageRef> result(loop_);
        result.resolve(message);
        return result;
      }
      break;
    }
  }
  event_loop::Deferred<MessageRef> result(loop_);
  waiters_.push_back(std::make_shared<Waiter>(
    Waiter{std::move(predicate),result}));
  return result;
}
std::function<void()> Mailbox::subscribe(
    std::shared_ptr<bootstrap::Signal<>::Listener> listener){
  requireOwner();return changed_.subscribe(std::move(listener));
}
} // namespace batbox::mailbox
