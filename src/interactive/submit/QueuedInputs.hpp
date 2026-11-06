#pragma once

#include "interactive/PresentationState.hpp"
#include "interactive/submit/CommandQueue.hpp"
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace batbox::interactive::submit {

// The row key of a queued command: its uuid, or its address while it has none.
std::string queuedCommandId(const CommandRef& command);
// The leader row for a command sent while a turn runs; nothing for Bat-scoped,
// background or meta commands, which never came from the poetry input.
std::optional<QueuedInput> describeQueuedCommand(const CommandRef& command);

// Keeps PresentationState::queuedInputs in step with the command queue and
// the transcript. A row is drawn from Enter until the message it turns into
// exists, so delivery leaves no gap and never shows the text twice.
class QueuedInputTracker {
public:
  // Each call returns true when the rows changed.
  bool syncQueue(const CommandList& queue,const std::vector<MessageRef>& messages);
  bool syncMessages(const std::vector<MessageRef>& messages);
  // The turn that took delivering rows has ended; their messages either
  // exist by now or never will (a hook blocked the poetry).
  bool settle();
  // These commands are about to leave the queue for the editor, not the model.
  void withdraw(const CommandList& commands);
  const std::vector<QueuedInput>& rows()const{return rows_;}
private:
  bool dropDelivered(const std::vector<MessageRef>& messages);
  std::vector<QueuedInput> rows_;
  std::unordered_set<std::string> withdrawn_,delivered_;
  // Messages before this index existed before the oldest row was queued.
  std::size_t scanFrom_=0;
};

// Takes queued leader poetry back into the input (the newest message, or all
// of them ahead of the current text), withdrawing its rows from delivery.
std::optional<PopEditableResult> takeQueuedPoetry(CommandQueue& queue,
  QueuedInputTracker& tracker,bool newestOnly,const std::string& input,
  std::size_t caret,double nowMs);

} // namespace batbox::interactive::submit
