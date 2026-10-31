#pragma once
#include "event_loop/Loop.hpp"
#include "model_stash/Detection.hpp"
#include "model_stash/DiffWriter.hpp"

namespace batbox::model_stash {

using StashCheckCallback=std::function<void(const std::string&,const Json&,
  const Json&,const Json&,const Json&,const std::optional<std::string>&)>;

// Binds the asynchronous diagnostic check to the shared query
// owner loop. State mutation stays on the owner, file I/O runs off-thread, and
// the post-await summary resumes in an owner microtask.
StashCheckCallback makeAsyncStashCheck(event_loop::Loop& loop,
    std::shared_ptr<Detector> detector,DiffWriterServices writer);

} // namespace batbox::model_stash
