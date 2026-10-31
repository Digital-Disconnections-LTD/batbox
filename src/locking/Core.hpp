#pragma once
#include "locking/FileOps.hpp"
#include <map>
#include <memory>

namespace batbox::locking {
struct Owned {
  std::string canonical,lockPath;
  Options options;
  double mtimeMs=0,lastUpdateMs=0;
  file_ops::Precision precision=file_ops::Precision::Milliseconds;
  std::optional<double> updateDelay;
  event_loop::Loop::TimerHandle timer;
  bool released=false;
};
struct Core:std::enable_shared_from_this<Core> {
  explicit Core(event_loop::Loop& loop):loop(loop){}
  event_loop::Loop& loop;
  std::map<std::string,std::shared_ptr<Owned>> locks;
  std::optional<file_ops::Precision> precision;
  bool alive=true;
  void schedule(const std::shared_ptr<Owned>& owned);
  void compromise(const std::shared_ptr<Owned>& owned,const Error& error);
  event_loop::Deferred<event_loop::Unit> unlockCanonical(
    const std::string& canonical);
  void unlockCanonicalSync(const std::string& canonical);
  void shutdown();
};
}
