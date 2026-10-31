#pragma once
#include "file_state_stash/FileStateStash.hpp"

namespace batbox::query {
Json filterDuplicateMemoryAttachments(const Json& attachments,
  file_state_stash::FileStateStash& readFileState);
} // namespace batbox::query
