#include "poetryEntries/Environment.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#ifndef _WIN32
#include <sys/utsname.h>
#endif

namespace batbox::poetryEntries {
std::optional<std::string> getKnowledgeCutoff(const std::string& model,
  const EnvironmentServices& services){
  if(!services.canonicalModelName)
    throw std::invalid_argument("Missing canonical model name binding");
  const auto canonical=services.canonicalModelName(model);
  for(const auto& pattern:services.knowledgePatterns)
    if(canonical.find(pattern.contains)!=std::string::npos)
      return pattern.cutoff;
  return std::nullopt;
}
std::string getShellInfoLine(const EnvironmentServices& services){
  const auto& value=value::property(services.environment,"SHELL");
  const auto shell=value.is_string()&&!value.get<std::string>().empty()
    ?value.get<std::string>():std::string("unknown");
  const auto shellName=shell.find("zsh")!=std::string::npos?"zsh":
    shell.find("bash")!=std::string::npos?"bash":shell;
  if(services.platform=="win32")
    return std::string("Shell: ")+shellName+
      " (use Unix shell syntax, not Windows — e.g., /dev/null not NUL, "
      "forward slashes in paths)";
  return std::string("Shell: ")+shellName;
}
std::string getUnameSR(const EnvironmentServices& services){
  if(services.platform=="win32"){
    if(!services.osVersion||!services.osRelease)
      throw std::invalid_argument("Missing Windows OS version binding");
    return services.osVersion()+" "+services.osRelease();
  }
#ifndef _WIN32
  struct utsname info{};
  if(::uname(&info)!=0)throw std::runtime_error("uname failed");
  return std::string(info.sysname)+" "+info.release;
#else
  throw std::invalid_argument("Windows environment requires win32 platform");
#endif
}
}
