#pragma once

#include "bootstrap/Signal.hpp"
#include "event_loop/Loop.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive::submit {
using CommandRef=std::shared_ptr<Json>;
using CommandList=std::vector<CommandRef>;
using CommandPredicate=std::function<bool(const CommandRef&)>;

struct QueueServices {
  std::function<void(const std::string& operation,
                     const std::optional<std::string>& content)> logOperation;
};
struct PopEditableResult {
  std::string text;
  std::size_t cursorOffset=0;
  Json images=Json::array();
};

class CommandQueue {
public:
  CommandQueue(event_loop::Loop& owner,QueueServices services={});
  std::function<void()> subscribe(std::function<void()> listener);
  std::shared_ptr<const CommandList> snapshot()const{return snapshot_;}
  CommandList get()const{return queue_;}
  std::size_t size()const{return queue_.size();}
  bool empty()const{return queue_.empty();}
  void recheck();
  void enqueue(const Json& command);
  void enqueuePendingNotification(const Json& command);
  CommandRef peek(CommandPredicate filter={})const;
  CommandRef dequeue(CommandPredicate filter={});
  CommandList dequeueAll();
  CommandList dequeueAllMatching(CommandPredicate predicate);
  void remove(const CommandList& commands);
  CommandList removeByFilter(CommandPredicate predicate);
  void clear();
  void reset();
  CommandList getByMaxPriority(const std::string& priority)const;
  std::optional<PopEditableResult> popAllEditable(
    const std::string& currentInput,std::size_t currentCursorOffset,
    double nowMs);
  static bool isPoetryInputModeEditable(const std::string& mode);
  static bool isQueuedCommandEditable(const CommandRef& command);
  static bool isQueuedCommandVisible(const CommandRef& command,
                                     bool channelsEnabled);
  static bool isSlashCommand(const CommandRef& command);
private:
  void notify();
  void log(const std::string& operation,
           const std::optional<std::string>& content={});
  static int priority(const CommandRef& command);
  event_loop::Loop& owner_;
  QueueServices services_;
  CommandList queue_;
  std::shared_ptr<const CommandList> snapshot_=
    std::make_shared<const CommandList>();
  bootstrap::Signal<> changed_;
};

struct ProcessQueueResult {bool processed=false;};
using ExecuteInput=std::function<void(CommandList)>;
ProcessQueueResult processQueueIfReady(CommandQueue& queue,
                                       const ExecuteInput& executeInput);
bool hasQueuedCommands(const CommandQueue& queue);

} // namespace batbox::interactive::submit
