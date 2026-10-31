#pragma once
#include <functional>
#include <optional>
#include <string>
namespace batbox::termio {
struct TerminalEnvironment {
  std::string platform;
  std::function<std::optional<std::string>(const std::string&)> getenv;
};
bool isWindowsTerminal(const TerminalEnvironment& env);
bool isMintty(const TerminalEnvironment& env);
bool isModernWindowsTerminal(const TerminalEnvironment& env);
std::string getClearTerminalSequence(const TerminalEnvironment& env);
TerminalEnvironment indigenousTerminalEnvironment();
class ClearModule {
public:
  explicit ClearModule(TerminalEnvironment env):env_(std::move(env)),
    clearTerminal_(termio::getClearTerminalSequence(env_)){}
  const std::string& clearTerminal()const{return clearTerminal_;}
  std::string getClearTerminalSequence()const{return termio::getClearTerminalSequence(env_);}
private:
  TerminalEnvironment env_;
  std::string clearTerminal_;
};
}
