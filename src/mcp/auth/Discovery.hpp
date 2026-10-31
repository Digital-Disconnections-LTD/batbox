#pragma once
#include "mcp/auth/Provider.hpp"

namespace batbox::mcp::auth {
struct Discovery {
  std::string authorizationServerUrl,resource;
  std::optional<std::string> resourceMetadataUrl;
  Json authorizationServerMetadata,resourceMetadata;
};
Json discoverAuthorizationServerMetadata(const std::string& issuer,
  const http::Fetch& fetch=http::fetch,std::stop_token signal={});
Discovery discoverOAuthServerInfo(const std::string& serverUrl,
  const Json& config,const std::optional<std::string>& resourceMetadataUrl={},
  const http::Fetch& fetch=http::fetch,std::stop_token signal={});
Json fetchAuthServerMetadata(const std::string& serverUrl,const Json& config,
  const std::optional<std::string>& resourceMetadataUrl={},
  const http::Fetch& fetch=http::fetch,std::stop_token signal={});
Json exchangeToken(const std::string& endpoint,Json form,const Json& client,
  const http::Fetch& fetch=http::fetch,std::stop_token signal={},const Json& metadata=Json::object());
std::string randomBase64Url(std::size_t size);
std::string sha256Base64Url(const std::string& bytes);
class OAuthFlight {
public:
  OAuthFlight(Provider& provider,http::Fetch fetch=http::fetch,
    std::stop_token signal={});
  std::string begin(const std::optional<std::string>& scope={},
    const std::optional<std::string>& resourceMetadataUrl={});
  void finish(const std::string& code);
private:
  Provider& provider_;
  http::Fetch fetch_;
  std::stop_token signal_;
  Discovery discovery_;
  Json client_;
};
} // namespace batbox::mcp::auth
