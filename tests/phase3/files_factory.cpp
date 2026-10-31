#include "gadgets/files/FileGadgets.hpp"
#include "filesystem/FileOps.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>

using batbox::Json;
namespace files=batbox::gadgets::files;
namespace fs=std::filesystem;

int main(){
  const auto dir=fs::temp_directory_path()/"batbox-file-factory-test";
  fs::remove_all(dir);fs::create_directories(dir);
  auto bindings=[](){
    files::FileGadgetBindings b;
    b.checkPermissions=[](const Json& input,const Json&){
      return std::async(std::launch::deferred,[input]{return Json{{"behavior","allow"},{"updatedInput",input}};});};
    b.matchWildcardPattern=[](const std::string& a,const std::string& b){return a==b;};
    b.userFacingName=[](const Json&){return std::string("file");};
    b.getGadgetUseSummary=[](const Json& input)->Json{return input.value("file_path",Json());};
    b.renderUse=[](const Json&,const Json&)->Json{return Json();};
    b.renderResult=[](const Json&,const Json&,const Json&)->Json{return Json();};
    b.renderError=[](const Json&,const Json&)->Json{return Json();};
    b.renderRejected=[](const Json&,const Json&)->Json{return Json();};
    b.renderTag=[](const Json&)->Json{return Json();};
    b.isResultTruncated=[](const Json&){return false;};
    return b;
  };
  auto paths=batbox::filesystem::PathServices{};
  paths.getCwd=[&]{return dir.string();};
  paths.homeDirectory=[&]{return dir.string();};
  paths.platform=[](){return std::string("linux");};
  paths.normalizeNFC=[](const std::string& p){return p;};
  files::FileReadServices read;read.paths=paths;
  read.isPDFSupported=[](){return true;};
  auto r=files::makeReadGadget(std::move(read),bindings());
  assert(r.name=="Read"&&r.inputJSONSchema["properties"].contains("pages"));
  assert(r.outputSchema["anyOf"].size()==6);
  assert(r.isConcurrencySafe(Json())&&r.isReadOnly(Json()));
  assert((r.isSearchOrReadCommand(Json())==Json{{"isSearch",false},{"isRead",true}}));
  assert(r.parseInput(Json{{"file_path","/tmp/a"},{"offset","2"}})["offset"]==2);
  assert(r.poetry({}).get().find("Batbox is a multimodal LLM")!=std::string::npos);
  Json backfill={{"file_path","~/x"}};r.backfillObservableInput(backfill);
  assert(backfill["file_path"]==dir.string()+"/x");

  batbox::file_state_stash::FileStateStash state(100);
  files::FileWriteServices write;write.paths=paths;write.readFileState=&state;
  write.checkTeamMemSecrets=[](const std::string&,const std::string&)->std::optional<std::string>{return std::nullopt;};
  write.matchingEditDenyRule=[](const std::string&){return false;};
  write.discoverSkillDirsForPaths=[](const std::string&,const std::string&){return std::vector<std::string>();};
  write.activateConditionalSkillsForPaths=[](const std::string&,const std::string&){};
  write.diagnosticBeforeFileEdited=[](const std::string&){};
  write.fileHistoryEnabled=[](){return false;};
  write.lspAvailable=[](){return false;};
  write.notifyEditorFileUpdated=[](const std::string&,const std::optional<std::string>&,const std::string&){};
  write.remoteDiffEnabled=[](){return false;};
  auto w=files::makeWriteGadget(std::move(write),bindings());
  assert(w.name=="Write"&&w.inputJSONSchema["required"].size()==2);
  assert(w.poetry({}).get().find("Read tool first")!=std::string::npos);
  const Json writeInput={{"file_path",(dir/"fresh.txt").string()},{"content","hello\n"}};
  assert(w.validateInput(writeInput,Json()).get().result);
  auto result=w.call(writeInput,{}, {},Json{{"uuid","parent-1"}},{}).get();
  assert(result.data["type"]=="create");
  assert(w.mapGadgetResultToGadgetResultBlockParam(result.data,"use-1").is_object());
  assert(fs::exists(dir/"fresh.txt"));

  files::FileEditServices edit;edit.paths=paths;
  edit.compactLinePrefixEnabled=[](){return false;};edit.conciseEditHint=[](){return false;};
  auto e=files::makeEditGadget(std::move(edit),bindings());
  assert(e.name=="Edit"&&e.inputJSONSchema["properties"].contains("replace_all"));
  assert(e.parseInput(Json{{"file_path","/tmp/a"},{"old_string","a"},
    {"new_string","b"},{"replace_all","false"}})["replace_all"]==false);
  assert(e.inputsEquivalent&&e.call&&e.validateInput);

  files::GlobServices glob;glob.paths=paths;
  auto g=files::makeGlobGadget(std::move(glob),bindings());
  assert(g.name=="Glob"&&g.isConcurrencySafe(Json()));
  assert((g.strict==std::nullopt&&g.isSearchOrReadCommand(Json())==Json{{"isSearch",true},{"isRead",false}}));
  assert(g.parseInput(Json{{"pattern","**/*.cpp"}})["pattern"]=="**/*.cpp");

  files::GrepServices grep;grep.paths=paths;
  auto p=files::makeGrepGadget(std::move(grep),bindings());
  assert(p.name=="Grep"&&p.strict.value());
  assert(p.parseInput(Json{{"pattern","hello"},{"head_limit","30"},{"-n","false"}})["head_limit"]==30);
  assert(p.poetry({}).get().find("SearXNG")==std::string::npos);
  bool bad=false;try{p.parseInput(Json{{"pattern","hello"},{"head_limit",""}});}catch(const std::invalid_argument&){bad=true;}
  assert(bad);
  fs::remove_all(dir);
  std::cout<<"file tool factories passed\n";
}
