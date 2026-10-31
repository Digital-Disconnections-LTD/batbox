#pragma once
#include "event_loop/Deferred.hpp"
#include "bootstrap/Signal.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::mailbox {
struct Message {
  std::string id,source,content,timestamp;
  std::optional<std::string> from,color;
};
using MessageRef=std::shared_ptr<Message>;
using Predicate=std::function<bool(const MessageRef&)>;
class Mailbox {
public:
  explicit Mailbox(event_loop::Loop& loop):loop_(loop){}
  std::size_t length()const;
  std::size_t revision()const;
  void send(MessageRef message);
  MessageRef poll(Predicate predicate={});
  event_loop::Deferred<MessageRef> receive(Predicate predicate={});
  std::function<void()> subscribe(
    std::shared_ptr<bootstrap::Signal<>::Listener> listener);
private:
  struct Waiter {
    Predicate predicate;
    event_loop::Deferred<MessageRef> result;
  };
  event_loop::Loop& loop_;
  std::vector<MessageRef> queue_;
  std::vector<std::shared_ptr<Waiter>> waiters_;
  bootstrap::Signal<> changed_;
  std::size_t revision_=0;
  void requireOwner()const;
  void notify();
  static bool matches(const Predicate& predicate,const MessageRef& message);
};
} // namespace batbox::mailbox
