#include "termio/Capabilities.hpp"
#ifdef _WIN32
#include <cstdio>
#include <io.h>
#else
#include <unistd.h>
#endif
namespace batbox::termio {
CapabilitiesServices indigenousCapabilitiesServices(
  std::function<std::optional<std::string>(const std::optional<std::string>&)> coerceVersion,
  std::function<bool(const std::string&,const std::string&)> versionGte,
  std::function<std::optional<std::string>()> terminalName){
  return {indigenousTerminalEnvironment(),[]{
#ifdef _WIN32
    return ::_isatty(::_fileno(stdout))!=0;
#else
    return ::isatty(STDOUT_FILENO)!=0;
#endif
  },
    std::move(coerceVersion),std::move(versionGte),std::move(terminalName)};
}
}
