#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::application_completion {
struct Suggestion {
  enum class Kind {Command,Path,Directory};
  Kind kind=Kind::Command;
  std::string display,description,replacement;
  std::size_t startUtf16=0,endUtf16=0;
  bool executeWithoutArgs=false;
};
struct Inputs {
  event_loop::Loop& owner;
  std::function<std::string()> cwd;
  std::function<Json()> commands;
  std::function<void(const std::string&)> log;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};
class Provider final:public std::enable_shared_from_this<Provider> {
public:
  static std::shared_ptr<Provider> create(Inputs inputs);
  event_loop::Deferred<std::vector<Suggestion>> suggest(
    const std::string& text,std::size_t caretUtf16);
  void clearPathStash();
private:
  explicit Provider(Inputs inputs):inputs_(std::move(inputs)){}
  struct PathEntry {std::string name;bool directory=false;};
  struct StashedPath {
    std::vector<PathEntry> entries;
    std::chrono::steady_clock::time_point loaded;
  };
  std::vector<PathEntry> scan(const std::string& directory,bool hidden);
  Inputs inputs_;
  std::mutex stashMutex_;
  std::unordered_map<std::string,StashedPath> pathStash_;
};
} // namespace batbox::application_completion
