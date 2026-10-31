#pragma once
#include "terminal_hooks/Notification.hpp"

namespace batbox::terminal_hooks {
enum class TabStatusKind {Idle,Busy,Waiting};
struct TabStatusServices:OscServices {
  std::function<bool()> supportsTabStatus;
};
std::string tabStatusPreset(TabStatusKind kind,
  const std::function<std::string(const Json&)>& osc);
std::string clearTabStatus(
  const std::function<std::string(const Json&)>& osc);
class TabStatusEffect {
public:
  void commit(std::optional<TabStatusKind> kind,WriteRawRef writeRaw,
    const TabStatusServices& services);
private:
  bool mounted_=false;
  std::optional<TabStatusKind> previousDependency_;
  std::optional<TabStatusKind> previousKindRef_;
  WriteRawRef previousWrite_;
};
}
