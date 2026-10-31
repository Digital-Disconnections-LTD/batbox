#include "settings/Validation.hpp"
#include <stdexcept>

namespace batbox::settings {
namespace {
std::string string(const Json& object,const char* key){
  const auto& item=value::property(object,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
bool is(const std::string& value,const char* expected){return value==expected;}
std::string link(const std::string& base,const std::string& suffix){
  if(base.empty())throw std::invalid_argument("Missing settings documentation base");
  return base+suffix;
}
}
ValidationTip getValidationTip(const Json& context,
  const std::string& docsBase){
  const auto path=string(context,"path"),code=string(context,"code"),
    expected=string(context,"expected");
  const auto& enumValues=value::property(context,"enumValues");
  ValidationTip tip;
  bool matched=true;
  if(is(path,"permissions.defaultMode")&&is(code,"invalid_value")){
    tip.suggestion="Valid modes: \"acceptEdits\" (ask before file changes), "
      "\"plan\" (analysis only), \"bypassPermissions\" (auto-accept all), "
      "or \"default\" (standard behavior)";
    tip.docLink=link(docsBase,"/iam#permission-modes");
  }else if(is(path,"apiKeyHelper")&&is(code,"invalid_type"))
    tip.suggestion="Provide a shell command that outputs your API key to "
      "stdout. The script should output only the API key. Example: "
      "\"/bin/generate_temp_api_key.sh\"";
  else if(is(path,"cleanupPeriodDays")&&is(code,"too_small")&&
          is(expected,"0"))
    tip.suggestion="Must be 0 or greater. Set a positive number for days to "
      "retain transcripts (default is 30). Setting 0 disables session "
      "persistence entirely: no transcripts are written and existing "
      "transcripts are deleted at startup.";
  else if(path.starts_with("env.")&&is(code,"invalid_type")){
    tip.suggestion="Environment variables must be strings. Wrap numbers and "
      "booleans in quotes. Example: \"DEBUG\": \"true\", \"PORT\": \"3000\"";
    tip.docLink=link(docsBase,"/settings#environment-variables");
  }else if((is(path,"permissions.allow")||is(path,"permissions.deny"))&&
           is(code,"invalid_type")&&is(expected,"array"))
    tip.suggestion="Permission rules must be in an array. Format: "
      "[\"Tool(specifier)\"]. Examples: [\"Bash(npm run build)\", "
      "\"Edit(docs/**)\", \"Read(~/.zshrc)\"]. Use * for wildcards.";
  else if(path.find("hooks")!=std::string::npos&&is(code,"invalid_type"))
    tip.suggestion="Hooks use a matcher + hooks array. The matcher is a "
      "string: a tool name (\"Bash\"), pipe-separated list "
      "(\"Edit|Write\"), or empty to match all. Example: "
      "{\"PostToolUse\": [{\"matcher\": \"Edit|Write\", "
      "\"hooks\": [{\"type\": \"command\", \"command\": \"echo Done\"}]}]}";
  else if(is(code,"invalid_type")&&is(expected,"boolean"))
    tip.suggestion="Use true or false without quotes. Example: "
      "\"includeCoAuthoredBy\": true";
  else if(is(code,"unrecognized_keys")){
    tip.suggestion="Check for typos or refer to the documentation for valid fields";
    tip.docLink=link(docsBase,"/settings");
  }else if(is(code,"invalid_value")&&enumValues.is_array()){
    // Start with an empty tip, then fill the enum suggestion.
  }else if(is(code,"invalid_type")&&is(expected,"object")&&
           context.is_object()&&context.contains("received")&&
           context["received"].is_null()&&path.empty())
    tip.suggestion="Check for missing commas, unmatched brackets, or trailing "
      "commas. Use a JSON validator to identify the exact syntax error.";
  else if(is(path,"permissions.additionalDirectories")&&
          is(code,"invalid_type")){
    tip.suggestion="Must be an array of directory paths. Example: "
      "[\"~/projects\", \"/tmp/workspace\"]. You can also use --add-dir "
      "flag or /add-dir command";
    tip.docLink=link(docsBase,"/iam#working-directories");
  }else matched=false;
  if(!matched)return {};
  if(is(code,"invalid_value")&&enumValues.is_array()&&
     (!tip.suggestion||tip.suggestion->empty())){
    std::string values;
    for(const auto& value:enumValues){
      if(!values.empty())values+=", ";
      values+='"'+(value.is_string()?value.get<std::string>():
        value::stringify(value))+'"';
    }
    tip.suggestion="Valid values: "+values;
  }
  if(!tip.docLink&&!path.empty()){
    const auto prefix=path.substr(0,path.find('.'));
    if(prefix=="permissions")tip.docLink=link(docsBase,
      "/iam#configuring-permissions");
    else if(prefix=="env")tip.docLink=link(docsBase,
      "/settings#environment-variables");
    else if(prefix=="hooks")tip.docLink=link(docsBase,"/hooks");
  }
  return tip;
}
}
