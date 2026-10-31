#pragma once
#include "auth/save/ApiKeySave.hpp"
#include "auth/storage/KeychainHelpers.hpp"

namespace batbox::auth::save {
void bindIndigenousApiKeyCommands(ApiKeySaveServices& services,
  event_loop::Loop& loop,
  std::shared_ptr<storage::KeychainState> state,
  storage::MacKeychainOps operations);
}
