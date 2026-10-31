#pragma once
#include "terminal_hooks/Notification.hpp"
#include "terminal_hooks/Title.hpp"
#include "terminal_hooks/TabStatus.hpp"
#include "termio/Capabilities.hpp"

namespace batbox::terminal_hooks {
OscServices indigenousOscServices(std::function<std::string()> terminalName,
  termio::TerminalEnvironment environment);
NotificationServices indigenousNotificationServices(
  OscServices osc,termio::CapabilitiesModule& capabilities);
TitleServices indigenousTitleServices(OscServices osc,
  termio::TerminalEnvironment environment,
  std::function<void(const std::string&)> setProcessTitle);
TabStatusServices indigenousTabStatusServices(OscServices osc,
  termio::TerminalEnvironment environment,
  const std::string& enabledUserType);
}
