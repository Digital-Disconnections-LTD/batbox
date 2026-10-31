#pragma once
#include "auth/OAuthTokens.hpp"
#include "auth/save/SaveTokens.hpp"
#include "auth/storage/Storage.hpp"

namespace batbox::auth::storage {
void bindOAuthStorage(OAuthTokenServices& tokens,
  save::Services& saveServices,const Storage& storage);
}
