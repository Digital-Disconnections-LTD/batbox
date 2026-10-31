#pragma once
#include "mcp/auth/Discovery.hpp"
#include "auth/storage/KeychainHelpers.hpp"
#include "locking/ProperLock.hpp"
#include <filesystem>

namespace batbox::mcp::auth {
struct IndigenousCredentialState;
struct IndigenousServices {
  Services services;
  std::shared_ptr<locking::Manager> locks;
  std::shared_ptr<::batbox::auth::storage::KeychainState> keychain;
  std::shared_ptr<IndigenousCredentialState> state;
};
IndigenousServices makeIndigenousServices(event_loop::Loop& loop,std::filesystem::path configHome,
  std::function<void(const std::string&)> debug={},http::Fetch fetch=http::fetch);
bool openBrowser(event_loop::Loop& owner,const std::string& url);
} // namespace batbox::mcp::auth
