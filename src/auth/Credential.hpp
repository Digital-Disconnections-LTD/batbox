#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth {
struct CredentialPaths {
  std::string oauthEnv="BATBOX_OAUTH_TOKEN_FILE_DESCRIPTOR";
  std::string apiKeyEnv="BATBOX_API_KEY_FILE_DESCRIPTOR";
  std::string oauthFile,apiKeyFile,tokenDirectory;
};
struct CredentialServices {
  CredentialPaths paths;
  std::function<std::optional<std::string>(const std::string&)> environment;
  std::function<std::string()> platform;
  std::function<bool()> remoteMode;
  std::function<std::string(const std::string&)> readFileSync;
  std::function<void(const std::string&,int)> mkdirRecursive;
  std::function<void(const std::string&,const std::string&,int)> writeFileSync;
  std::function<void(const std::string&,const std::string&)> debug;
};
class CredentialReader {
public:
  explicit CredentialReader(CredentialServices services);
  void maybePersistTokenForSubprocesses(const std::string& path,
    const std::string& token,const std::string& tokenName);
  std::optional<std::string> readTokenFromWellKnownFile(
    const std::string& path,const std::string& tokenName);
  std::optional<std::string> getOAuthTokenFromFileDescriptor();
  std::optional<std::string> getApiKeyFromFileDescriptor();
  void clearStashedFileDescriptors();
private:
  CredentialServices services_;
  std::optional<std::optional<std::string>> oauthStashed_,apiKeyStashed_;
  std::optional<std::string> getCredentialFromFd(
    const std::string& envVar,const std::string& wellKnownPath,
    const std::string& label,
    std::optional<std::optional<std::string>>& stashed);
};
CredentialServices indigenousCredentialServices(CredentialPaths paths,
  std::function<std::optional<std::string>(const std::string&)> environment,
  std::function<bool()> remoteMode,
  std::function<void(const std::string&,const std::string&)> debug);
}
