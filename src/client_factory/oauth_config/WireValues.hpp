#pragma once
#include "client_factory/oauth_config/OAuthConfig.hpp"

namespace batbox::client_factory::oauth_config {
Data makeArchivedProviderOAuthData(bool internalAtModuleLoad);
}
