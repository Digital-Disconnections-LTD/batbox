#pragma once
#include <string>
#include <vector>

namespace batbox::instructions::hooks::runtime {
struct GuardedAddress {
  std::string address;
  int family=0;
};
bool isBlockedHookAddress(const std::string& address);
std::vector<GuardedAddress> guardedHookLookup(const std::string& hostname);
} // namespace batbox::instructions::hooks::runtime
