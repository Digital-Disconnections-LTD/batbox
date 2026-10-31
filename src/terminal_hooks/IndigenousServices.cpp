#include "terminal_hooks/IndigenousServices.hpp"
#include "cleanup/Terminal.hpp"
#include "npm_wrap_ansi/Internal.hpp"
#include "runtime/JsonValue.hpp"
#include "termio/OscLink.hpp"
#include <stdexcept>

namespace batbox::terminal_hooks {
namespace {
bool present(const std::optional<std::string>& value){
  return value&&!value->empty();
}
}
OscServices indigenousOscServices(std::function<std::string()> terminalName,
    termio::TerminalEnvironment environment){
  if(!terminalName||!environment.getenv)
    throw std::invalid_argument("Terminal name/environment imports required");
  return {
    [terminalName=std::move(terminalName)](const Json& parts){
      return termio::osc(parts,terminalName());},
    [environment=std::move(environment)](const std::string& sequence){
      return cleanup::indigenousWrapForMultiplexer(sequence,
        present(environment.getenv("TMUX")),
        present(environment.getenv("STY")));}
  };
}
NotificationServices indigenousNotificationServices(OscServices osc,
    termio::CapabilitiesModule& capabilities){
  NotificationServices services;
  services.osc=std::move(osc.osc);
  services.wrapForMultiplexer=std::move(osc.wrapForMultiplexer);
  services.isProgressReportingAvailable=[&capabilities]{
    return capabilities.isProgressReportingAvailable();};
  return services;
}
TitleServices indigenousTitleServices(OscServices osc,
    termio::TerminalEnvironment environment,
    std::function<void(const std::string&)> setProcessTitle){
  TitleServices services;
  services.osc=std::move(osc.osc);
  services.wrapForMultiplexer=std::move(osc.wrapForMultiplexer);
  services.stripAnsi=[](const std::string& input){
    return value::fromUtf16(npm_wrap_ansi::stripAnsi(
      value::utf16CodeUnits(input)));};
  services.isWindows=[platform=environment.platform]{
    return platform=="win32";};
  services.setProcessTitle=std::move(setProcessTitle);
  return services;
}
TabStatusServices indigenousTabStatusServices(OscServices osc,
    termio::TerminalEnvironment environment,
    const std::string& enabledUserType){
  if(!environment.getenv||enabledUserType.empty())
    throw std::invalid_argument("Tab status environment/profile required");
  TabStatusServices services;
  services.osc=std::move(osc.osc);
  services.wrapForMultiplexer=std::move(osc.wrapForMultiplexer);
  services.supportsTabStatus=[environment=std::move(environment),
      enabledUserType]{
    return environment.getenv("USER_TYPE")==enabledUserType;};
  return services;
}
}
