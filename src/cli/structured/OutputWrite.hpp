#pragma once
#include "cli/util/Ndjson.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <string>

namespace batbox::cli::structured {
using StdoutSink=std::function<void(const std::string&)>;
event_loop::Deferred<event_loop::Unit> write(
  event_loop::Loop& loop,const Json& message,
  const cli::NdjsonStringifier& stringify,const StdoutSink& sink);
}
