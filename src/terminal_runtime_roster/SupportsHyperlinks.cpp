#include "terminal_runtime_roster/HyperlinkSdk.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <array>
#include <cwchar>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef _WIN32
extern char** environ;
#endif
namespace batbox::terminal_runtime_roster {
namespace {
const std::array<const char*,6> additional={
  "ghostty","Hyper","kitty","alacritty","iTerm.app","iTerm2"};
bool includes(const std::string& value){
  return std::any_of(additional.begin(),additional.end(),
    [&](const char* candidate){return value==candidate;});
}
std::string value(const Environment& env,const char* key){
  const auto it=env.find(key);return it==env.end()?"":it->second;
}
}

HyperlinkSupport::HyperlinkSupport(DetectionContext context,
    std::function<Environment()> currentProcessEnv):
  currentProcessEnv_(std::move(currentProcessEnv)),
  libraryStdout_(createSupportsHyperlinks(context)){}

bool HyperlinkSupport::supportsHyperlinks(
    const SupportsHyperlinksOptions& options)const{
  const bool stdoutSupported=options.stdoutSupported.value_or(libraryStdout_);
  if(stdoutSupported)return true;
  if(!options.env&&!currentProcessEnv_)
    throw std::logic_error("Current process env import required");
  const Environment liveEnv=options.env?Environment{}:currentProcessEnv_();
  const auto& env=options.env?*options.env:liveEnv;
  const auto program=value(env,"TERM_PROGRAM");
  if(!program.empty()&&includes(program))return true;
  const auto lcTerminal=value(env,"LC_TERMINAL");
  if(!lcTerminal.empty()&&includes(lcTerminal))return true;
  const auto term=value(env,"TERM");
  if(term.find("kitty")!=std::string::npos)return true;
  return false;
}

DetectionContext captureIndigenousDetectionContext(
    std::vector<std::string> processArgv){
  DetectionContext context;
  context.argv=std::move(processArgv);
#ifdef _WIN32
  context.stdoutIsTTY=::_isatty(::_fileno(stdout))!=0;
#else
  context.stdoutIsTTY=::isatty(STDOUT_FILENO)!=0;
#endif
#ifdef _WIN32
  context.platform="win32";
#elif defined(__APPLE__)
  context.platform="darwin";
#else
  context.platform="linux";
#endif
  context.env=captureIndigenousEnvironment();
  return context;
}

Environment captureIndigenousEnvironment(){
  Environment env;
#ifdef _WIN32
  wchar_t* block=::GetEnvironmentStringsW();
  if(!block)return env;
  for(const wchar_t* entry=block;*entry;entry+=std::wcslen(entry)+1){
    const std::wstring wide(entry);
    const auto text=runtime::pathToUtf8(std::filesystem::path(wide));
    const auto equal=text.find('=');
    if(equal!=std::string::npos&&equal>0)
      env.emplace(text.substr(0,equal),text.substr(equal+1));
  }
  ::FreeEnvironmentStringsW(block);
#else
  for(char** entry=environ;entry&&*entry;++entry){
    const std::string text=*entry;
    const auto equal=text.find('=');
    if(equal!=std::string::npos)
      env.emplace(text.substr(0,equal),text.substr(equal+1));
  }
#endif
  return env;
}

HyperlinkSupport makeIndigenousHyperlinkSupport(
    std::vector<std::string> processArgv){
  return HyperlinkSupport(captureIndigenousDetectionContext(
    std::move(processArgv)),captureIndigenousEnvironment);
}
}
