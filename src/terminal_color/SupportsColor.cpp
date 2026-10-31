#include "terminal_color/ColorDetection.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <regex>
#include <stdexcept>

namespace batbox::terminal_color {
namespace {
bool hasFlag(const std::string& flag,
    const std::vector<std::string>& argv){
  const auto prefix=flag.starts_with("-")?"":flag.size()==1?"-":"--";
  const auto wanted=std::string(prefix)+flag;
  auto position=argv.size(),terminator=argv.size();
  for(std::size_t i=0;i<argv.size();i++){
    if(argv[i]==wanted&&position==argv.size())position=i;
    if(argv[i]=="--"&&terminator==argv.size())terminator=i;
  }
  return position<argv.size()&&position<terminator;
}
bool hasAny(const ColorDetectionInputs& in,
    std::initializer_list<const char*> names){
  for(const auto name:names)if(in.getenv(name))return true;
  return false;
}
double number(const std::string& value){
  if(value.empty())return 0;
  try{return std::stod(value);}catch(...){
    return std::numeric_limits<double>::quiet_NaN();
  }
}
double osPart(const std::string& release,std::size_t index){
  std::size_t begin=0;
  for(std::size_t i=0;i<index;i++){
    const auto dot=release.find('.',begin);
    if(dot==std::string::npos)return std::numeric_limits<double>::quiet_NaN();
    begin=dot+1;
  }
  const auto end=release.find('.',begin);
  return number(release.substr(begin,end==std::string::npos?end:end-begin));
}
}
double detectColorLevel(const ColorDetectionInputs& in){
  if(!in.getenv)throw std::invalid_argument("Color environment import required");
  std::optional<double> flagForceColor;
  if(hasFlag("no-color",in.argv)||hasFlag("no-colors",in.argv)||
      hasFlag("color=false",in.argv)||hasFlag("color=never",in.argv))
    flagForceColor=0;
  else if(hasFlag("color",in.argv)||hasFlag("colors",in.argv)||
      hasFlag("color=true",in.argv)||hasFlag("color=always",in.argv))
    flagForceColor=1;
  const auto forceEnv=in.getenv("FORCE_COLOR");
  std::optional<double> noFlagForceColor;
  if(forceEnv){
    if(*forceEnv=="true")noFlagForceColor=1;
    else if(*forceEnv=="false")noFlagForceColor=0;
    else if(forceEnv->empty())noFlagForceColor=1;
    else{
      const auto parsed=value::parseInt10(*forceEnv);
      noFlagForceColor=std::isnan(parsed)?parsed:std::min(parsed,3.0);
    }
    flagForceColor=noFlagForceColor;
  }
  const auto forceColor=in.sniffFlags?flagForceColor:noFlagForceColor;
  if(forceColor&&*forceColor==0)return 0;
  if(in.sniffFlags){
    if(hasFlag("color=16m",in.argv)||hasFlag("color=full",in.argv)||
        hasFlag("color=truecolor",in.argv))return 3;
    if(hasFlag("color=256",in.argv))return 2;
  }
  if(in.getenv("TF_BUILD")&&in.getenv("AGENT_NAME"))return 1;
  if(in.haveStream&&!in.streamIsTTY&&!forceColor)return 0;
  const auto min=forceColor&&*forceColor!=0&&!std::isnan(*forceColor)?
    *forceColor:0;
  if(in.getenv("TERM")==std::optional<std::string>("dumb"))return min;
  if(in.platform=="win32"){
    const auto major=osPart(in.osRelease,0),build=osPart(in.osRelease,2);
    if(major>=10&&build>=10586)return build>=14931?3:2;
    return 1;
  }
  if(in.getenv("CI")){
    if(hasAny(in,{"GITHUB_ACTIONS","GITEA_ACTIONS","CIRCLECI"}))return 3;
    if(hasAny(in,{"TRAVIS","APPVEYOR","GITLAB_CI","BUILDKITE",
          "DRONE"})||in.getenv("CI_NAME")==
          std::optional<std::string>("codeship"))return 1;
    return min;
  }
  if(const auto teamcity=in.getenv("TEAMCITY_VERSION")){
    static const std::regex pattern(R"(^(9\.(0*[1-9]\d*)\.|\d{2,}\.))");
    return std::regex_search(*teamcity,pattern)?1:0;
  }
  if(in.getenv("COLORTERM")==std::optional<std::string>("truecolor"))return 3;
  const auto term=in.getenv("TERM");
  if(term=="xterm-kitty"||term=="xterm-ghostty"||term=="wezterm")return 3;
  if(const auto program=in.getenv("TERM_PROGRAM")){
    const auto versionString=in.getenv("TERM_PROGRAM_VERSION").value_or("");
    const auto version=value::parseInt10(versionString.substr(0,
      versionString.find('.')));
    if(*program=="iTerm.app")return version>=3?3:2;
    if(*program=="Apple_Terminal")return 2;
  }
  const auto termText=term.value_or("undefined");
  static const std::regex color256(R"(-256(color)?$)",std::regex::icase);
  if(std::regex_search(termText,color256))return 2;
  static const std::regex basic(
    R"(^screen|^xterm|^vt100|^vt220|^rxvt|color|ansi|cygwin|linux)",
    std::regex::icase);
  if(std::regex_search(termText,basic))return 1;
  if(in.getenv("COLORTERM"))return 1;
  return min;
}
} // namespace batbox::terminal_color
