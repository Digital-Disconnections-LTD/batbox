#include "settings/Sources.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::settings {
std::string sourceKey(Source source){
  switch(source){
    case Source::User:return "userSettings";
    case Source::Project:return "projectSettings";
    case Source::Local:return "localSettings";
    case Source::Flag:return "flagSettings";
    case Source::Policy:return "policySettings";
  }
  throw std::invalid_argument("Unknown settings source");
}
Source sourceFromKey(const std::string& key){
  for(const auto source:settingSources())if(sourceKey(source)==key)return source;
  throw std::invalid_argument("Unknown settings source: "+key);
}
std::vector<Source> settingSources(){
  return {Source::User,Source::Project,Source::Local,Source::Flag,Source::Policy};
}
std::string getSettingSourceName(Source source){
  switch(source){
    case Source::User:return "user";
    case Source::Project:return "project";
    case Source::Local:return "project, gitignored";
    case Source::Flag:return "cli flag";
    case Source::Policy:return "managed";
  }
  throw std::invalid_argument("Unknown settings source");
}
std::string getSourceDisplayName(const std::string& source){
  if(source=="plugin")return "Plugin";
  if(source=="built-in")return "Built-in";
  switch(sourceFromKey(source)){
    case Source::User:return "User";
    case Source::Project:return "Project";
    case Source::Local:return "Local";
    case Source::Flag:return "Flag";
    case Source::Policy:return "Managed";
  }
  throw std::invalid_argument("Unknown settings source");
}
std::string getSettingSourceDisplayNameLowercase(const std::string& source){
  if(source=="cliArg")return "CLI argument";
  if(source=="command")return "command configuration";
  if(source=="session")return "current session";
  switch(sourceFromKey(source)){
    case Source::User:return "user settings";
    case Source::Project:return "shared project settings";
    case Source::Local:return "project local settings";
    case Source::Flag:return "command line arguments";
    case Source::Policy:return "enterprise managed settings";
  }
  throw std::invalid_argument("Unknown settings source");
}
std::string getSettingSourceDisplayNameCapitalized(const std::string& source){
  if(source=="cliArg")return "CLI argument";
  if(source=="command")return "Command configuration";
  if(source=="session")return "Current session";
  switch(sourceFromKey(source)){
    case Source::User:return "User settings";
    case Source::Project:return "Shared project settings";
    case Source::Local:return "Project local settings";
    case Source::Flag:return "Command line arguments";
    case Source::Policy:return "Enterprise managed settings";
  }
  throw std::invalid_argument("Unknown settings source");
}
std::vector<Source> parseSettingSourcesFlag(const std::string& flag){
  std::vector<Source> result;
  if(flag.empty())return result;
  std::size_t start=0;
  while(true){
    const auto end=flag.find(',',start);
    const auto name=value::trim(flag.substr(start,end==std::string::npos
      ?std::string::npos:end-start));
    if(name=="user")result.push_back(Source::User);
    else if(name=="project")result.push_back(Source::Project);
    else if(name=="local")result.push_back(Source::Local);
    else throw std::invalid_argument("Invalid setting source: "+name+
      ". Valid options are: user, project, local");
    if(end==std::string::npos)break;
    start=end+1;
  }
  return result;
}
std::vector<Source> getEnabledSettingSources(const std::vector<Source>& allowed){
  std::vector<Source> result;
  for(const auto source:allowed)
    if(std::find(result.begin(),result.end(),source)==result.end())
      result.push_back(source);
  for(const auto source:{Source::Policy,Source::Flag})
    if(std::find(result.begin(),result.end(),source)==result.end())
      result.push_back(source);
  return result;
}
bool isSettingSourceEnabled(Source source,const std::vector<Source>& allowed){
  const auto enabled=getEnabledSettingSources(allowed);
  return std::find(enabled.begin(),enabled.end(),source)!=enabled.end();
}
std::vector<Source> editableSources(){
  return {Source::Local,Source::Project,Source::User};
}
}
