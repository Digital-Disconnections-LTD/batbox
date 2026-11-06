#include "gadgets/bash/Schema.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <regex>
#include <stdexcept>
#include <string>

namespace batbox::bash_gadget {
namespace {
const std::regex decimal(R"(^-?\d+(\.\d+)?$)");
[[noreturn]] void invalid(const std::string& field){throw std::invalid_argument("Invalid Bash input field: "+field);}
Json semanticNumber(const Json& value){
  if(!value.is_string())return value;
  const auto& text=value.get_ref<const std::string&>();
  if(!std::regex_match(text,decimal))return value;
  try{const double number=std::stod(text);if(std::isfinite(number))return number;}
  catch(...){/* Leave an uncoercible string unchanged for numeric validation. */}
  return value;
}
Json semanticBoolean(const Json& value){
  if(value=="true")return true;
  if(value=="false")return false;
  return value;
}
// A local model often sends cwd, workdir or cmd; name the accepted fields so
// the retry is right the first time.
[[noreturn]] void unknown(const std::string& field,bool backgroundMissionsDisabled){
  if(field=="run_in_background")throw std::invalid_argument("Invalid Bash input field: "
    "run_in_background. Background commands are disabled here; run the command in the "
    "foreground.");
  throw std::invalid_argument("Invalid Bash input field: "+field+". Bash accepts only "+
    (backgroundMissionsDisabled?"command, timeout and description. ":
      "command, timeout, description and run_in_background. ")+
    "To run in another directory, start the command with cd <dir> &&.");
}
void checkString(const Json& value,const std::string& key){if(!value.is_string())invalid(key);}
void checkBoolean(const Json& value,const std::string& key){if(!value.is_boolean())invalid(key);}
}

Json parseBashInput(const Json& input,bool backgroundMissionsDisabled,bool allowInternalSedEdit){
  if(!input.is_object())invalid("<object>");
  for(auto it=input.begin();it!=input.end();++it){
    const auto& key=it.key();
    if(key!="command"&&key!="timeout"&&key!="description"&&
       !(key=="run_in_background"&&!backgroundMissionsDisabled)&&
       !(key=="_simulatedSedEdit"&&allowInternalSedEdit))unknown(key,backgroundMissionsDisabled);
  }
  if(!input.contains("command"))invalid("command");
  Json result=input;
  checkString(result.at("command"),"command");
  if(result.contains("description"))checkString(result.at("description"),"description");
  if(result.contains("timeout")){
    result["timeout"]=semanticNumber(result.at("timeout"));
    if(!result["timeout"].is_number()||
       !std::isfinite(result["timeout"].get<double>()))invalid("timeout");
  }
  if(result.contains("run_in_background")){
    result["run_in_background"]=semanticBoolean(result.at("run_in_background"));
    checkBoolean(result.at("run_in_background"),"run_in_background");
  }
  if(result.contains("_simulatedSedEdit")){
    const auto& edit=result.at("_simulatedSedEdit");
    if(!edit.is_object()||!edit.contains("filePath")||!edit.contains("newContent"))invalid("_simulatedSedEdit");
    checkString(edit.at("filePath"),"_simulatedSedEdit.filePath");
    checkString(edit.at("newContent"),"_simulatedSedEdit.newContent");
    result["_simulatedSedEdit"]={{"filePath",edit.at("filePath")},
      {"newContent",edit.at("newContent")}};
  }
  return result;
}

Json bashInputJSONSchema(double maxTimeoutMs,bool backgroundMissionsDisabled){
  Json properties={{"command",{{"description","The command to execute"},{"type","string"}}},
    {"timeout",{{"description","Optional timeout in milliseconds (max "+value::numberToString(maxTimeoutMs)+")"},{"type","number"}}},
    {"description",{{"description",R"(Clear, concise description of what this command does in active voice. Never use words like "complex" or "risk" in the description - just describe what it does.

For simple commands (git, npm, standard CLI tools), keep it brief (5-10 words):
- ls → "List files in current directory"
- git status → "Show working tree status"
- npm install → "Install package dependencies"

For commands that are harder to parse at a glance (piped commands, obscure flags, etc.), add enough context to clarify what it does:
- find . -name "*.tmp" -exec rm {} \; → "Find and delete all .tmp files recursively"
- git reset --hard origin/main → "Discard all local changes and match remote main"
- curl -s url | jq '.data[]' → "Fetch JSON from URL and extract data array elements")"},{"type","string"}}}};
  if(!backgroundMissionsDisabled)properties["run_in_background"]={{"description","Set to true to run this command in the background. Use Read to read the output later."},{"type","boolean"}};
  return Json{{"$schema","https://json-schema.org/draft/2020-12/schema"},{"type","object"},
    {"properties",properties},{"required",Json::array({"command"})},{"additionalProperties",false}};
}
Json bashOutputJSONSchema(){
  Json properties=Json::object();
  const std::pair<const char*,const char*> fields[]={
    {"stdout","The standard output of the command"},{"stderr","The standard error output of the command"},
    {"rawOutputPath","Path to raw output file for large MCP tool outputs"},
    {"interrupted","Whether the command was interrupted"},{"isImage","Flag to indicate if stdout contains image data"},
    {"backgroundTaskId","ID of the background task if command is running in background"},
    {"backgroundedByUser","True if the user manually backgrounded the command with Ctrl+B"},
    {"assistantAutoBackgrounded","True if assistant-mode auto-backgrounded a long-running blocking command"},
    {"returnCodeInterpretation","Semantic interpretation for non-error exit codes with special meaning"},
    {"noOutputExpected","Whether the command is expected to produce no output on success"},
    {"structuredContent","Structured content blocks"},
    {"persistedOutputPath","Path to the persisted full output in tool-results dir (set when output is too large for inline)"},
    {"persistedOutputSize","Total size of the output in bytes (set when output is too large for inline)"},
    {"persistedOutputTail","The end of an output too large for inline"}};
  for(const auto& [name,description]:fields){
    Json prop={{"description",description},{"type",std::string(name)=="interrupted"||
      std::string(name)=="isImage"||std::string(name)=="backgroundedByUser"||
      std::string(name)=="assistantAutoBackgrounded"||
      std::string(name)=="noOutputExpected"?"boolean":std::string(name)=="persistedOutputSize"?"number":
      std::string(name)=="structuredContent"?"array":"string"}};
    if(std::string(name)=="structuredContent")prop["items"]=Json::object();
    properties[name]=std::move(prop);
  }
  return Json{{"$schema","https://json-schema.org/draft/2020-12/schema"},{"type","object"},
    {"properties",properties},{"required",Json::array({"stdout","stderr","interrupted"})},
    {"additionalProperties",false}};
}
} // namespace batbox::bash_gadget
