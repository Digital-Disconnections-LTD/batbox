#pragma once
#include "event_loop/Loop.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <memory>

namespace batbox::interactive::text_input {
struct BufferEntry {
  std::string text;
  double cursorOffset=0;
  std::shared_ptr<Json> pastedContents;
  double timestamp=0;
};
using BufferEntryRef=std::shared_ptr<BufferEntry>;
struct InputBufferServices {
  using Cancel=std::function<void()>;
  std::function<double()> now;
  std::function<Cancel(double,std::function<void()>)> setTimeout;
};
class InputBuffer {
public:
  struct State;
  InputBuffer(std::int64_t maxSize,double debounceMs,InputBufferServices services);
  ~InputBuffer();
  InputBuffer(const InputBuffer&)=delete;
  InputBuffer& operator=(const InputBuffer&)=delete;
  void push(std::string text,double cursorOffset,std::shared_ptr<Json> pastedContents={});
  BufferEntryRef undo();
  void clear();
  bool canUndo()const;
  std::int64_t index()const;
  const std::vector<BufferEntryRef>& entries()const;
private:
  std::shared_ptr<State> state_;
};
InputBufferServices indigenousInputBufferServices(event_loop::Loop& owner);
} // namespace batbox::interactive::text_input
