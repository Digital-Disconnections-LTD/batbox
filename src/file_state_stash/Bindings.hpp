#pragma once
#include "file_state_stash/FileStateStash.hpp"

namespace batbox::gadgets::files {
struct FileReadServices;
struct FileWriteServices;
}
namespace batbox::query_engine {struct AskServices;}
namespace batbox::file_state_stash {
void bindReadStash(gadgets::files::FileReadServices& services,
  std::shared_ptr<FileStateStash> stash);
void bindWriteStash(gadgets::files::FileWriteServices& services,
  std::shared_ptr<FileStateStash> stash);
void bindAskStash(query_engine::AskServices& services);
} // namespace batbox::file_state_stash
