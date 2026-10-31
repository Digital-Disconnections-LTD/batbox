#pragma once
#include "auth/oauth_client/AccountActions.hpp"
#include "auth/save/ApiKeySave.hpp"
#include <memory>

namespace batbox::auth::oauth_client {
AccountServices makeIndigenousAccountServices(event_loop::Loop& loop,
  client_factory::oauth_config::Services config,
  std::function<Json()> getGlobalConfig,
  std::function<void(const std::function<Json(const Json&)>&)>
    saveGlobalConfig,
  std::shared_ptr<auth::save::ApiKeySaver> saver);
}
