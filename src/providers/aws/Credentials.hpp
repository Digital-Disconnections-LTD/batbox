#pragma once
#include "providers/aws/SigV4.hpp"
#include "providers/cloud/Clock.hpp"
#include "providers/cloud/Command.hpp"
#include "transport/Client.hpp"
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace batbox::providers::aws {
struct BedrockAuth {
  enum class Kind { SigV4, Bearer };
  Kind kind = Kind::SigV4;
  Credentials credentials;
  std::string bearerToken;
  // Where the credentials came from, for diagnostics. Never contains secrets.
  std::string source;
};
using IniFile = std::map<std::string,std::map<std::string,std::string>>;
IniFile parseIni(std::string_view text);
std::filesystem::path homeDirectory();
std::filesystem::path sharedConfigPath();
std::filesystem::path sharedCredentialsPath();
// Parses credential_process output (Version 1 JSON). Returns nullopt and sets error on failure.
std::optional<Credentials> parseProcessCredentials(std::string_view output, std::string& error);
struct CredentialOptions {
  // Bedrock API key configured on the endpoint; it wins over everything else.
  std::string bearerToken;
  // Profile to use; empty means AWS_PROFILE, then "default".
  std::string profile;
  cloud::SystemClock now = [] { return std::chrono::system_clock::now(); };
  std::function<cloud::CommandResult(const std::string&)> runCommand = cloud::runCommand;
  // Network settings for STS AssumeRole calls.
  transport::ClientConfig network;
  // Credentials are refreshed this long before they expire.
  std::chrono::seconds refreshMargin{300};
};
// Resolution order: endpoint bearer token, AWS_BEARER_TOKEN_BEDROCK,
// AWS_ACCESS_KEY_ID/AWS_SECRET_ACCESS_KEY[/AWS_SESSION_TOKEN], then the profile from
// ~/.aws/credentials and ~/.aws/config (role_arn with source_profile, static keys,
// credential_process). Results are stashed until shortly before they expire.
class CredentialChain {
  struct Stashed {
    BedrockAuth auth;
    std::optional<std::chrono::system_clock::time_point> expiration;
  };
  CredentialOptions options;
  mutable std::mutex mutex;
  std::optional<Stashed> stash;
  std::map<std::string,Credentials> processStash;
  Stashed resolveUncached();
  std::map<std::string,std::string> profileSettings(const std::string& name) const;
  Credentials resolveProfile(const std::string& name, std::set<std::string>& visiting, std::string& source);
  Credentials runCredentialProcess(const std::string& command);
  Credentials assumeRole(const std::map<std::string,std::string>& settings, const Credentials& sourceCredentials);
  bool fresh(const std::optional<std::chrono::system_clock::time_point>& expiration) const;
public:
  explicit CredentialChain(CredentialOptions options = {});
  BedrockAuth resolve();
  void invalidate();
  std::string profileName() const;
  // AWS_REGION, AWS_DEFAULT_REGION, then the profile's region; empty when none is set.
  std::string region() const;
};
} // namespace batbox::providers::aws
