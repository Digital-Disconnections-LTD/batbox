#pragma once
#include "event_loop/Deferred.hpp"
#include "flights/AsyncLoad.hpp"
#include "flights/Paths.hpp"
#include <functional>
#include <string>

namespace batbox::cli::headless {
struct UnresolvedGadgetUseServices {
  std::function<std::string()> getTranscriptPath;
  std::function<event_loop::Deferred<flights::LoadedTranscript>(const std::string&)> loadTranscriptFile;
};

event_loop::Deferred<Json> findUnresolvedGadgetUse(
  event_loop::Loop& loop,const std::string& gadgetUseId,
  const UnresolvedGadgetUseServices& services);

// Binds the existing indigenous transcript path and asynchronous JSONL loader.
UnresolvedGadgetUseServices bindIndigenousUnresolvedGadgetUse(
  event_loop::Loop& loop,flights::Paths& paths,
  const flights::AsyncLoadServices& loadServices);
}
