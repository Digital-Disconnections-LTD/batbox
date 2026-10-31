#include "gadgets/files/FileWrite.hpp"
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  const std::string op=t.at("op").get<std::string>();
  if(op=="map")return Json{{"result",files::mapWriteResult(t.at("data"),t.at("toolUseID").get<std::string>())}};
  const Json& input=t.at("input");
  const std::string path=input.at("file_path").get<std::string>();
  batbox::file_state_stash::FileStateStash state(100);
  if(std::filesystem::exists(path)&&t.value("readMode",std::string())!="none"){
    const std::string content=batbox::filesystem::readFileSyncWithMetadata(path).content;
    const std::string mode=t.value("readMode",std::string());
    state.set(path,files::ReadSnapshot{content,batbox::filesystem::getFileModificationTime(path)-(mode=="stale"?1:0),
      mode=="partial",mode=="partial"?std::optional<double>(0):std::nullopt,
      mode=="partial"?std::optional<double>(1):std::nullopt});
  }
  std::vector<std::string> events;
  std::set<std::string> triggers;
  files::FileWriteServices s;
  s.readFileState=&state;s.dynamicSkillDirTriggers=&triggers;
  s.paths.getCwd=[&](){return t.at("cwd").get<std::string>();};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& path){return path;};
  s.checkTeamMemSecrets=[](const std::string&,const std::string& content)->std::optional<std::string>{
    return content.find("secret")!=std::string::npos?std::optional<std::string>("Secret blocked"):std::nullopt;};
  s.matchingEditDenyRule=[&](const std::string&){return t.value("deny",false);};
  s.discoverSkillDirsForPaths=[&](const std::string&,const std::string&){
    std::vector<std::string> dirs;
    if(t.contains("skillDirs"))for(const auto& item:t.at("skillDirs"))dirs.push_back(item.get<std::string>());
    return dirs;};
  s.addSkillDirectories=[&](const std::vector<std::string>& dirs){
    std::string joined;for(const auto& d:dirs){if(!joined.empty())joined+=',';joined+=d;}events.push_back("addSkills:"+joined);};
  s.activateConditionalSkillsForPaths=[&](const std::string&,const std::string&){events.push_back("activateSkills");};
  s.diagnosticBeforeFileEdited=[&](const std::string&){events.push_back("diagnosticBefore");};
  s.fileHistoryEnabled=[&](){return t.value("history",false);};
  s.fileHistoryTrackEdit=[&](const std::string&,const std::string& uuid){events.push_back("history:"+uuid);};
  s.getPatchForDisplay=[](const std::string& path,const std::string& old,const std::string& next){
    return Json::array({Json{{"filePath",path},{"old",old},{"new",next}}});};
  s.notifyEditorFileUpdated=[&](const std::string&,const std::optional<std::string>&,const std::string&){events.push_back("notifyEditor");};
  s.lspAvailable=[&](){return t.value("lsp",false);};
  s.clearDeliveredDiagnostics=[&](const std::string&){events.push_back("clearDiagnostics");};
  s.lspChangeFile=[&](const std::string&,const std::string&){events.push_back("lspChange");};
  s.lspSaveFile=[&](const std::string&){events.push_back("lspSave");};
  s.remoteDiffEnabled=[&](){return t.value("remote",false);};
  s.fetchSingleFileGitDiff=[&](const std::string&)->std::optional<Json>{
    if(t.contains("gitDiff")&&!t.at("gitDiff").is_null())return t.at("gitDiff");
    return std::nullopt;};
  if(op=="validate"){
    const auto result=files::validateWriteInput(input,s);
    Json value={{"result",result.result}};
    if(result.message)value["message"]=*result.message;
    if(result.errorCode)value["errorCode"]=*result.errorCode;
    return Json{{"result",value}};
  }
  const batbox::gadgets::GadgetCallContext context{Json::object(),std::stop_token{}};
  const auto result=files::callWrite(input,"parent-1",context,s);
  Json triggered=Json::array();for(const auto& item:triggers)triggered.push_back(item);
  const auto found=state.get(path);
  return Json{{"result",Json{{"data",result.data}}},{"events",events},{"triggered",triggered},
    {"stateContent",found?Json(found->content):Json()}};
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
  try{std::cout<<batbox::value::stringify(evaluate(batbox::value::parse(input)))<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}
