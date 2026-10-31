#pragma once
#include "settings/remote/SyncStash.hpp"

namespace batbox::settings::remote {
struct EligibilityServices {
  std::function<std::string()> provider;
  std::function<bool()> isPrimaryBaseUrl;
  std::function<std::string()> entrypoint;
  std::function<Json()> oauthTokens;
  std::function<std::optional<std::string>()> apiKeyWithoutHelper;
  std::string primaryProvider="firstParty";
  std::string localBatEntrypoint="local-agent";
  std::string inferenceScope;
};
class Eligibility {
public:
  Eligibility(SyncStash& stash,EligibilityServices services);
  bool isRemoteManagedSettingsEligible();
  void resetSyncStash();
private:
  SyncStash& stash_;
  EligibilityServices services_;
  std::optional<bool> stashed_;
};
}
