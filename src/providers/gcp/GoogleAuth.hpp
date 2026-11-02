#pragma once
#include "providers/cloud/Clock.hpp"
#include "providers/cloud/Command.hpp"
#include "runtime/JsonValue.hpp"
#include "transport/Client.hpp"
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace batbox::providers::gcp {
inline constexpr const char* cloudPlatformScope = "https://www.googleapis.com/auth/cloud-platform";
struct AccessToken {
  std::string token;
  std::optional<std::chrono::system_clock::time_point> expiration;
  std::string source;
};
// Builds the RS256 JWT a service account exchanges for an access token (RFC 7523).
std::string serviceAccountAssertion(const Json& serviceAccount, std::chrono::system_clock::time_point now);
std::filesystem::path wellKnownCredentialsPath();
struct GoogleAuthOptions {
  // GOOGLE_APPLICATION_CREDENTIALS when empty.
  std::string credentialsFile;
  std::string gcloudCommand = "gcloud auth print-access-token";
  cloud::SystemClock now = [] { return std::chrono::system_clock::now(); };
  std::function<cloud::CommandResult(const std::string&)> runCommand = cloud::runCommand;
  transport::ClientConfig network;
  // Tokens are refreshed this long before they expire.
  std::chrono::seconds refreshMargin{60};
  // gcloud does not report expiry, so its token is reused for this long (and dropped on a 401).
  std::chrono::seconds gcloudLifetime{900};
};
// Application Default Credentials, in order: GOOGLE_APPLICATION_CREDENTIALS (service_account or
// authorized_user JSON), the gcloud application-default file, `gcloud auth print-access-token`,
// then the metadata server (GCE_METADATA_HOST, default 169.254.169.254).
class GoogleTokenSource {
  GoogleAuthOptions options;
  std::mutex mutex;
  std::optional<AccessToken> stash;
  AccessToken fetch();
  AccessToken exchange(const std::string& tokenUri, const std::string& form, const std::string& source);
  AccessToken fromCredentialsFile(const std::filesystem::path& path);
  std::optional<AccessToken> fromGcloud();
  std::optional<AccessToken> fromMetadata();
public:
  explicit GoogleTokenSource(GoogleAuthOptions options = {});
  AccessToken token();
  void invalidate();
  // project_id from the credentials file, when one is in use.
  std::string credentialsProject() const;
};
} // namespace batbox::providers::gcp
