#pragma once
#include "event_loop/Deferred.hpp"
#include "gadgets/bash/BashCompletion.hpp"

namespace batbox::bash_gadget {
struct AsyncBashFileOps {
  std::function<event_loop::Deferred<std::uintmax_t>(const std::string&)> stat;
  std::function<event_loop::Deferred<event_loop::Unit>()> ensureDir;
  std::function<std::string(const std::string&,bool)> getGadgetResultPath;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&,
    std::uintmax_t)> truncate;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&,
    const std::string&)> link,copyFile;
};
// Persist large Bash output through owner-loop continuations. Rejection
// retains the already observed stat size.
event_loop::Deferred<BashPersistedOutput> persistBashOutputAsync(
  event_loop::Loop& loop,const shell::ExecResult& result,
  const AsyncBashFileOps& operations,
  std::function<void(const BashPersistedOutput&)> onComplete={});
// Real indigenous filesystem imports. getGadgetResultPath remains an owner callback;
// resultsDir is the directory ensured before that callback is invoked.
AsyncBashFileOps indigenousBashFileOps(event_loop::Loop& loop,std::string resultsDir,
  std::function<std::string(const std::string&,bool)> getGadgetResultPath);
} // namespace batbox::bash_gadget
