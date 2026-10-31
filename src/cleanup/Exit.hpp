#pragma once
#include <functional>

namespace batbox::cleanup {
struct ExitServices {
  std::function<void()> clearFailsafe,drainStdin,killSelf;
  std::function<void(int)> processExit;
  std::function<bool()> isTestMode;
};
void forceExit(int code,const ExitServices& services);
std::function<void(int)> indigenousProcessExit();
std::function<void()> indigenousKillSelf();
} // namespace batbox::cleanup
