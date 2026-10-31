#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::locking {
struct Core;
struct Error:std::runtime_error {
  std::string code,file;
  Error(std::string code,std::string message,std::string file={}):
    std::runtime_error(message),code(std::move(code)),file(std::move(file)){}
};
struct RetryOptions {
  int retries=0;
  double minTimeoutMs=1000,
    maxTimeoutMs=std::numeric_limits<double>::infinity(),factor=2;
  bool randomize=false;
};
struct Options {
  double staleMs=10000;
  std::optional<double> updateMs;
  bool realpath=true;
  std::optional<std::string> lockfilePath;
  RetryOptions retry;
  std::function<void(const Error&)> onCompromised;
};
using Release=std::function<event_loop::Deferred<event_loop::Unit>()>;
class Manager {
public:
  explicit Manager(event_loop::Loop& loop);
  ~Manager();
  Manager(const Manager&)=delete;
  Manager& operator=(const Manager&)=delete;
  event_loop::Deferred<Release> lock(const std::string& file,
    Options options={});
  event_loop::Deferred<event_loop::Unit> unlock(const std::string& file,
    Options options={});
  event_loop::Deferred<bool> check(const std::string& file,
    Options options={});
  std::function<void()> lockSync(const std::string& file,
    Options options={});
  void unlockSync(const std::string& file,Options options={});
  bool checkSync(const std::string& file,Options options={});
private:
  std::shared_ptr<Core> core_;
};
}
