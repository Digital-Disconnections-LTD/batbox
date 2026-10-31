#pragma once
#include <functional>
#include <optional>
#include <string>
namespace batbox::cli {
struct ExitServices {
  std::function<void(const std::string&)> consoleError,stdoutWrite;
  std::function<void(int)> processExit;
};
// Returning lets an injected exit callback observe the request in tests.
void cliError(const std::optional<std::string>& message,const ExitServices& services);
void cliOk(const std::optional<std::string>& message,const ExitServices& services);
ExitServices indigenousExitServices();
}
