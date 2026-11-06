#pragma once
#include <functional>
#include <optional>
#include <string>
namespace batbox::termio {
struct TerminalEnvironment {
  std::string platform;
  std::function<std::optional<std::string>(const std::string&)> getenv;
};
TerminalEnvironment indigenousTerminalEnvironment();
}
