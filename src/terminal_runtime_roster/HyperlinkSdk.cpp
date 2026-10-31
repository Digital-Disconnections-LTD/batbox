#include "terminal_runtime_roster/HyperlinkSdk.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <regex>

namespace batbox::terminal_runtime_roster {
namespace {
bool present(const Environment& env,const char* key){return env.contains(key);}
std::string value(const Environment& env,const char* key){
  const auto it=env.find(key);return it==env.end()?"":it->second;
}
bool anyFlag(const std::vector<std::string>& argv,
    std::initializer_list<const char*> flags){
  return std::any_of(flags.begin(),flags.end(),[&](const char* flag){
    return hasFlag(flag,argv);});
}
struct Version {double major,minor,patch;};
Version parseVersion(const std::string& input){
  static const std::regex compact(R"(^\d{3,4}$)");
  if(std::regex_match(input,compact)){
    static const std::regex pieces(R"((\d{1,2})(\d{2}))");
    std::smatch match;
    std::regex_search(input,match,pieces);
    return {0,value::parseInt10(match[1].str()),
      value::parseInt10(match[2].str())};
  }
  const auto first=input.find('.');
  const auto second=first==std::string::npos?first:
    input.find('.',first+1);
  return {value::parseInt10(input.substr(0,first)),
    first==std::string::npos?value::parseInt10(""):
      value::parseInt10(input.substr(first+1,second-first-1)),
    second==std::string::npos?value::parseInt10(""):
      value::parseInt10(input.substr(second+1,
        input.find('.',second+1)-second-1))};
}
}

bool createSupportsHyperlinks(const DetectionContext& context){
  const auto& env=context.env;
  const auto& argv=context.argv;
  const auto force=value(env,"FORCE_HYPERLINK");
  if(!force.empty())
    return !(value::parseInt10(force)==0);
  if(anyFlag(argv,{"no-hyperlink","no-hyperlinks","hyperlink=false",
      "hyperlink=never"}))return false;
  if(anyFlag(argv,{"hyperlink=true","hyperlink=always"}))return true;
  if(!value(env,"NETLIFY").empty())return true;
  if(!createSupportsColor(context))return false;
  if(!context.stdoutIsTTY)return false;
  if(present(env,"WT_SESSION"))return true;
  if(context.platform=="win32")return false;
  if(!value(env,"CI").empty())return false;
  if(!value(env,"TEAMCITY_VERSION").empty())return false;
  const auto termProgram=value(env,"TERM_PROGRAM");
  if(!termProgram.empty()){
    const auto versionText=value(env,"TERM_PROGRAM_VERSION");
    const auto version=parseVersion(versionText);
    if(termProgram=="iTerm.app"){
      if(version.major==3)return version.minor>=1;
      return version.major>3;
    }
    if(termProgram=="WezTerm"){
      static const std::regex unstable(
        R"(^0-unstable-\d{4}-\d{2}-\d{2}$)");
      if(std::regex_match(versionText,unstable))
        return versionText.substr(std::string("0-unstable-").size())>=
          "2020-06-20";
      return version.major>=20200620;
    }
    if(termProgram=="vscode"){
      if(!value(env,"CURSOR_TRACE_ID").empty())return true;
      return version.major>1||
        (version.major==1&&version.minor>=72);
    }
    if(termProgram=="ghostty"||termProgram=="zed"||
        termProgram=="Orca")return true;
  }
  const auto vte=value(env,"VTE_VERSION");
  if(!vte.empty()){
    if(vte=="0.50.0")return false;
    const auto version=parseVersion(vte);
    return version.major>0||version.minor>=50;
  }
  const auto term=value(env,"TERM");
  return term=="alacritty"||term=="xterm-kitty";
}
}
