#pragma once
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
namespace batbox::termio {
struct TerminalEnvironment {
  std::string platform;
  std::function<std::optional<std::string>(const std::string&)> getenv;
};
inline TerminalEnvironment indigenousTerminalEnvironment(){
  TerminalEnvironment env;
#if defined(_WIN32)
  env.platform="win32";
#elif defined(__APPLE__)
  env.platform="darwin";
#else
  env.platform="linux";
#endif
  env.getenv=[](const std::string& key)->std::optional<std::string>{
    const auto value=std::getenv(key.c_str());if(!value)return {};return std::string(value);
  };
  return env;
}
}
