#include "gadgets/files/FileEdit.hpp"
#include "formatting/Format.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <iostream>
using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  batbox::file_state_stash::FileStateStash state(100);
  const auto path=t.at("input").at("file_path").get<std::string>();
  const auto mode=t.value("readMode",std::string());
  if(std::filesystem::exists(path)&&!mode.empty()&&mode!="none"){
    const auto content=batbox::filesystem::readFileSyncWithMetadata(path).content;
    state.set(path,files::ReadSnapshot{content,batbox::filesystem::getFileModificationTime(path)-
      (mode=="stale"||mode=="range"?1:0),mode=="partial",
      mode=="range"?std::optional<double>(1):std::nullopt,
      mode=="range"?std::optional<double>(1):std::nullopt});
  }
  files::FileEditServices s;s.readFileState=&state;
  s.paths.getCwd=[&](){return t.at("cwd").get<std::string>();};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.checkTeamMemSecrets=[](const std::string&,const std::string& content)->std::optional<std::string>{
    return content.find("secret")!=std::string::npos?std::optional<std::string>("Secret blocked"):std::nullopt;};
  s.matchingEditDenyRule=[&](const std::string&){return t.value("deny",false);};
  s.findSimilarFile=[&](const std::string&){return t.value("similar",std::string());};
  s.suggestPathUnderCwd=[&](const std::string&)->std::optional<std::string>{
    if(t.contains("suggestion"))return t.at("suggestion").get<std::string>();return std::nullopt;};
  s.validateInputForSettingsFileEdit=[&](const std::string&,const std::string&,
      const std::function<std::string()>& simulate)->std::optional<batbox::gadgets::ValidationResult>{
    if(!t.contains("settingsValidation"))return std::nullopt;
    (void)simulate();const auto& v=t.at("settingsValidation");
    batbox::gadgets::ValidationResult result;result.result=v.at("result").get<bool>();
    if(v.contains("message"))result.message=v.at("message").get<std::string>();
    if(v.contains("errorCode"))result.errorCode=v.at("errorCode").get<int>();
    if(v.contains("behavior"))result.behavior=v.at("behavior").get<std::string>();
    if(v.contains("meta"))result.meta=v.at("meta");return result;};
  s.formatFileSize=batbox::formatting::formatFileSize;
  const auto result=files::validateEditInput(t.at("input"),s);
  Json output={{"result",result.result}};
  if(result.behavior)output["behavior"]=*result.behavior;
  if(result.message)output["message"]=*result.message;
  if(result.errorCode)output["errorCode"]=*result.errorCode;
  if(!result.meta.is_null())output["meta"]=result.meta;
  return output;
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
