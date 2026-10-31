#pragma once
#include "context/AsyncContext.hpp"
#include <stdexcept>

namespace batbox::context {
struct AsyncContextStash::State {
  event_loop::Loop::Dispatcher owner;
  std::optional<event_loop::Deferred<std::optional<std::string>>> git;
  std::optional<event_loop::Deferred<Json>> system,user;
  std::shared_ptr<std::optional<std::string>> injection=
    std::make_shared<std::optional<std::string>>();
  explicit State(event_loop::Loop& loop):owner(loop.dispatcher()){}
};
template<class Fn> void requireContextBinding(const Fn& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Missing context binding: ")+name);
}
void checkContextOwner(const event_loop::Loop::Dispatcher& owner);
std::string formatGitStatus(const std::vector<std::string>&);
}
