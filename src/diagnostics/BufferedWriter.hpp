#pragma once
#include "event_loop/Loop.hpp"
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace batbox::diagnostics {
struct BufferedWriterServices {
  std::function<void(const std::string&)> writeFn;
  std::function<std::function<void()>(double,std::function<void()>)> setTimeout;
  std::function<void(std::function<void()>)> setImmediate;
  double flushIntervalMs=1000;
  std::size_t maxBufferSize=100;
  double maxBufferBytes=std::numeric_limits<double>::infinity();
  bool immediateMode=false;
};
class BufferedWriter {
public:
  explicit BufferedWriter(BufferedWriterServices services);
  void write(const std::string& content);
  void flush();
  void dispose();
private:
  struct State;
  std::shared_ptr<State> state_;
};
BufferedWriterServices indigenousBufferedWriterServices(event_loop::Loop& loop,
    std::function<void(const std::string&)> writeFn);
}
