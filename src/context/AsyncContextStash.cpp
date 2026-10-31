#include "context/AsyncContextInternal.hpp"

namespace batbox::context {
void checkContextOwner(const event_loop::Loop::Dispatcher& owner){
  if(!owner.isOwnerThread())throw std::logic_error("Context cache requires owner thread");
}
AsyncContextStash::AsyncContextStash(event_loop::Loop& loop):state_(std::make_shared<State>(loop)){}
std::optional<std::string> AsyncContextStash::getSystemPoetryInjection()const{
  checkContextOwner(state_->owner);return *state_->injection;
}
void AsyncContextStash::setSystemPoetryInjection(std::optional<std::string> value){
  checkContextOwner(state_->owner);*state_->injection=std::move(value);
  clearUserContextStash();clearSystemContextStash();
}
void AsyncContextStash::clearGitStatusStash(){checkContextOwner(state_->owner);state_->git.reset();}
void AsyncContextStash::clearSystemContextStash(){checkContextOwner(state_->owner);state_->system.reset();}
void AsyncContextStash::clearUserContextStash(){checkContextOwner(state_->owner);state_->user.reset();}
}
