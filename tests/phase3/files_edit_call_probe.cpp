#include "gadgets/files/FileEdit.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <iostream>
using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  if(t.value("op",std::string())=="map")return files::mapEditResult(t.at("data"),"use-1");
  const auto path=t.at("input").at("file_path").get<std::string>();
  const auto mode=t.value("readMode",std::string());
  batbox::file_state_stash::FileStateStash state(100);
  if(std::filesystem::exists(path)&&!mode.empty()&&mode!="none"){
    const auto content=batbox::filesystem::readFileSyncWithMetadata(path).content;
    state.set(path,files::ReadSnapshot{content,batbox::filesystem::getFileModificationTime(path)-
      (mode=="stale"?1:0),mode=="partial",
      mode=="range"?std::optional<double>(1):std::nullopt,
      mode=="range"?std::optional<double>(1):std::nullopt});
  }
  std::vector<std::string> events;
  std::set<std::string> triggers;
  files::FileEditServices s;s.readFileState=&state;s.dynamicSkillDirTriggers=&triggers;
  s.paths.getCwd=[&](){return t.at("cwd").get<std::string>();};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.simpleMode=[&](){return t.value("simple",true);};
  s.discoverSkillDirsForPaths=[&](const std::string&,const std::string&){
    std::vector<std::string> dirs;if(t.contains("skillDirs"))for(const auto& x:t.at("skillDirs"))dirs.push_back(x.get<std::string>());
    return dirs;};
  s.addSkillDirectories=[&](const std::vector<std::string>& dirs){
    std::string all;for(const auto& d:dirs){if(!all.empty())all+=',';all+=d;}events.push_back("addSkills:"+all);};
  s.activateConditionalSkillsForPaths=[&](const std::string&,const std::string&){events.push_back("activateSkills");};
  s.diagnosticBeforeFileEdited=[&](const std::string&){events.push_back("diagnosticBefore");};
  s.fileHistoryEnabled=[&](){return t.value("history",false);};
  s.fileHistoryTrackEdit=[&](const std::string&,const std::string& uuid){events.push_back("history:"+uuid);};
  s.editUtils.getPatchForDisplay=[](const std::string& p,const std::string& old,const std::string& next){
    return Json::array({Json{{"filePath",p},{"old",old},{"new",next}}});};
  s.editUtils.getPatchFromContents=s.editUtils.getPatchForDisplay;
  s.lspAvailable=[&](){return t.value("lsp",false);};
  s.clearDeliveredDiagnostics=[&](const std::string&){events.push_back("clearDiagnostics");};
  s.lspChangeFile=[&](const std::string&,const std::string&){events.push_back("lspChange");};
  s.lspSaveFile=[&](const std::string&){events.push_back("lspSave");};
  s.notifyEditorFileUpdated=[&](const std::string&,const std::optional<std::string>&,
     const std::string&){events.push_back("notifyEditor");};
  s.remoteDiffEnabled=[&](){return t.value("remote",false);};
  s.fetchSingleFileGitDiff=[&](const std::string&)->std::optional<Json>{
    if(t.contains("gitDiff"))return t.at("gitDiff");return std::nullopt;};
  batbox::gadgets::GadgetCallContext context{};context.data=Json::object();
  if(t.contains("userModified"))context.data["userModified"]=t.at("userModified");
  const auto result=files::callEdit(t.at("input"),"parent-1",context,s);
  Json triggered=Json::array();for(const auto& dir:triggers)triggered.push_back(dir);
  Json snapshot;
  if(auto found=state.get(path))
    snapshot=Json{{"content",found->content},{"timestamp",found->timestamp}};
  return Json{{"data",result.data},{"events",events},{"triggered",triggered},{"state",snapshot}};
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
