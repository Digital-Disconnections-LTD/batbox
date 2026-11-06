#include "termio/Clear.hpp"
#include <cstdlib>
namespace batbox::termio {
TerminalEnvironment indigenousTerminalEnvironment(){
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
