#pragma once
#include "terminal_hooks/Notification.hpp"

namespace batbox::terminal_hooks {
struct TitleServices:OscServices {
  std::function<std::string(const std::string&)> stripAnsi;
  std::function<bool()> isWindows;
  std::function<void(const std::string&)> setProcessTitle;
};
class TerminalTitleEffect {
public:
  void commit(std::optional<std::string> title,WriteRawRef writeRaw,
    const TitleServices& services);
private:
  bool mounted_=false;
  std::optional<std::string> previousTitle_;
  WriteRawRef previousWrite_;
};
}
