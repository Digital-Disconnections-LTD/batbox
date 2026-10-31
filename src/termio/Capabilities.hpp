#pragma once
#include "termio/Clear.hpp"
namespace batbox::termio {
struct CapabilitiesServices {
  TerminalEnvironment environment;
  std::function<bool()> stdoutIsTTY;
  std::function<std::optional<std::string>(const std::optional<std::string>&)> coerceVersion;
  std::function<bool(const std::string&,const std::string&)> versionGte;
  std::function<std::optional<std::string>()> terminalName;
};
class CapabilitiesModule {
public:
  explicit CapabilitiesModule(CapabilitiesServices services);
  bool isProgressReportingAvailable()const;
  bool isSynchronizedOutputSupported()const;
  void setXtversionName(const std::string& name);
  bool isXtermJs()const;
  bool supportsExtendedKeys()const;
  bool hasCursorUpViewportYankBug()const;
  bool syncOutputSupported()const{return syncOutputSupported_;}
private:
  CapabilitiesServices services_;
  std::optional<std::string> xtversionName_;
  bool syncOutputSupported_;
};
CapabilitiesServices indigenousCapabilitiesServices(
  std::function<std::optional<std::string>(const std::optional<std::string>&)> coerceVersion,
  std::function<bool(const std::string&,const std::string&)> versionGte,
  std::function<std::optional<std::string>()> terminalName);
}
