#include "terminal_runtime_roster/HyperlinkSdk.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <regex>

namespace batbox::terminal_runtime_roster {
namespace {
bool contains(const Environment& env,const char* key){return env.contains(key);}
std::string value(const Environment& env,const char* key){
  const auto it=env.find(key);return it==env.end()?"":it->second;
}
bool anyFlag(const std::vector<std::string>& argv,
    std::initializer_list<const char*> flags){
  return std::any_of(flags.begin(),flags.end(),[&](const char* flag){
    return hasFlag(flag,argv);});
}
}

bool hasFlag(const std::string& flag,const std::vector<std::string>& argv){
  const std::string prefix=flag.starts_with('-')?"":
    (flag.size()==1?"-":"--");
  const auto flagPos=std::find(argv.begin(),argv.end(),prefix+flag);
  const auto terminator=std::find(argv.begin(),argv.end(),"--");
  return flagPos!=argv.end()&&
    (terminator==argv.end()||flagPos<terminator);
}

bool createSupportsColor(const DetectionContext& context){
  const auto& env=context.env;
  const auto& argv=context.argv;
  std::optional<int> flagForceColor;
  if(anyFlag(argv,{"no-color","no-colors","color=false",
      "color=never"}))flagForceColor=0;
  else if(anyFlag(argv,{"color","colors","color=true",
      "color=always"}))flagForceColor=1;

  std::optional<int> envForceColor;
  if(contains(env,"FORCE_COLOR")){
    const auto force=value(env,"FORCE_COLOR");
    if(force=="true"||force.empty())envForceColor=1;
    else if(force=="false")envForceColor=0;
    else{
      const auto parsed=std::min(value::parseInt10(force),3.0);
      if(std::isfinite(parsed)&&parsed>=0&&parsed<=3)
        envForceColor=static_cast<int>(parsed);
    }
  }
  if(envForceColor)flagForceColor=envForceColor;
  const auto forceColor=flagForceColor;
  if(forceColor==0)return false;
  if(anyFlag(argv,{"color=16m","color=full","color=truecolor",
      "color=256"}))return true;
  if(contains(env,"TF_BUILD")&&contains(env,"AGENT_NAME"))return true;
  if(!context.stdoutIsTTY&&!forceColor)return false;
  const int min=forceColor.value_or(0);
  if(value(env,"TERM")=="dumb")return min>0;
  if(context.platform=="win32")return true;
  if(contains(env,"CI")){
    if(contains(env,"GITHUB_ACTIONS")||contains(env,"GITEA_ACTIONS")||
       contains(env,"CIRCLECI")||contains(env,"TRAVIS")||
       contains(env,"APPVEYOR")||contains(env,"GITLAB_CI")||
       contains(env,"BUILDKITE")||contains(env,"DRONE")||
       value(env,"CI_NAME")=="codeship")return true;
    return min>0;
  }
  if(contains(env,"TEAMCITY_VERSION")){
    static const std::regex teamcity(R"(^(9\.(0*[1-9]\d*)\.|\d{2,}\.))");
    return std::regex_search(value(env,"TEAMCITY_VERSION"),teamcity);
  }
  if(value(env,"COLORTERM")=="truecolor")return true;
  const auto term=value(env,"TERM");
  if(term=="xterm-kitty"||term=="xterm-ghostty"||term=="wezterm")
    return true;
  if(contains(env,"TERM_PROGRAM")){
    const auto version=value(env,"TERM_PROGRAM_VERSION");
    const auto major=value::parseInt10(version.substr(0,version.find('.')));
    if(value(env,"TERM_PROGRAM")=="iTerm.app")return true;
    if(value(env,"TERM_PROGRAM")=="Apple_Terminal")return true;
    (void)major; // Both supported branches return a positive color level.
  }
  static const std::regex color256(R"(-256(color)?$)",std::regex::icase);
  static const std::regex basic(R"(^screen|^xterm|^vt100|^vt220|^rxvt|color|ansi|cygwin|linux)",
    std::regex::icase);
  return std::regex_search(term,color256)||std::regex_search(term,basic)||
    contains(env,"COLORTERM")||min>0;
}
}
