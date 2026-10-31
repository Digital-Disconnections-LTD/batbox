#include "terminal_color/ColorDetection.hpp"
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <io.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace batbox::terminal_color {
ColorDetectionInputs indigenousColorDetectionInputs(
    const std::vector<std::string>& argv){
  ColorDetectionInputs result;
  result.getenv=[](const std::string& name)->std::optional<std::string>{
    if(const auto* value=std::getenv(name.c_str()))return std::string(value);
    return {};
  };
  result.argv=argv;
#if defined(_WIN32)
  result.platform="win32";
#elif defined(__APPLE__)
  result.platform="darwin";
#else
  result.platform="linux";
#endif
#ifdef _WIN32
  OSVERSIONINFOW version{};version.dwOSVersionInfoSize=sizeof(version);
  if(::GetVersionExW(&version))result.osRelease=
    std::to_string(version.dwMajorVersion)+"."+
    std::to_string(version.dwMinorVersion)+"."+
    std::to_string(version.dwBuildNumber);
#else
  struct utsname name{};
  if(::uname(&name)==0)result.osRelease=name.release;
#endif
  result.haveStream=true;
#ifdef _WIN32
  result.streamIsTTY=::_isatty(::_fileno(stdout))!=0;
#else
  result.streamIsTTY=::isatty(STDOUT_FILENO)==1;
#endif
  result.sniffFlags=true;
  return result;
}
ChalkState initializeIndigenousChalkState(
    const std::vector<std::string>& argv,bool tmuxTruecolor){
  const auto in=indigenousColorDetectionInputs(argv);
  return initializeChalkState(detectColorLevel(in),
    in.getenv("TERM_PROGRAM"),in.getenv("TMUX"),tmuxTruecolor);
}
} // namespace batbox::terminal_color
