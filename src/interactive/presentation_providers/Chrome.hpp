#pragma once

#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/presentation_providers/Shortcut.hpp"
#include "interactive/presentation_providers/ThemeState.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::presentation_providers {

// Values are read for every paint so a settings or model change takes effect
// without rebuilding the terminal driver.
struct ChromeServices {
  ThemeState& theme;
  const ShortcutProvider& shortcuts;
  std::function<std::string()> modelLabel;
  std::function<std::string()> workingDirectory;
  std::function<std::string()> permissionMode;
  std::function<std::string()> statusText;
  // Optional context for the status row and welcome card. Empty selectors
  // hide their part.
  std::function<std::string()> gitBranch;
  std::function<std::optional<double>()> contextUsage;
  std::function<std::string()> homeDirectory;
  // One muted line under the welcome facts, such as missing web helpers.
  std::function<std::string()> notice;
  // Clock for Mission elapsed times; the system clock when empty.
  std::function<long long()> nowMillis;
  // The dim line under the Bats list, such as updateReminder's; none when empty.
  std::string reminder;
};

// "Batbox gets updated frequently", followed by how this copy updates when its
// canonical executable path shows how it was installed (Homebrew or the
// release installer). Reads nothing but the path it is given.
std::string updateReminder(const std::string& executablePath);

conversation_shell::ChromeProviders makeChromeProviders(ChromeServices services);

} // namespace batbox::interactive::presentation_providers
