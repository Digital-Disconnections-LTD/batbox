#include "application_gadgets/IndigenousFiles.hpp"
#include "application_gadgets/IndigenousFileHelpers.hpp"
#include "gadgets/files/Notebook.hpp"
#include "services/tokenEstimation.hpp"
#include "instructions/MemoryScan.hpp"
#include "formatting/Format.hpp"
#include "runtime/Unicode.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::application_gadgets {
namespace {
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Application file import required: ")+name);
}
filesystem::PathServices paths(const Inputs& input){
  need(input.cwd,"cwd");need(input.homeDirectory,"homeDirectory");
  filesystem::PathServices p;
  p.getCwd=input.cwd;p.homeDirectory=input.homeDirectory;
  p.platform=[]{return std::string("linux");};
  p.posixPathToWindowsPath=[](const std::string& path){return path;};
  p.normalizeNFC=value::normalizeNFC;
  return p;
}
void requireFiles(const Files& f){
  if(!f.stash)throw std::invalid_argument("Application file cache required");
  if(!f.skillRoster)throw std::invalid_argument("Application skill registry required");
  need(f.runOnOwnerAndWait,"skill owner dispatch");
  need(f.readDenied,"read deny rule");need(f.editDenied,"edit deny rule");
  need(f.readIgnorePatterns,"read ignore patterns");
  need(f.normalizeIgnorePatterns,"normalize ignore patterns");
  need(f.pluginGlobExclusions,"plugin glob exclusions");
  need(f.patch,"structured file patch");need(f.countTokens,"model token counter");
  need(f.notebookFormatOutput,"notebook output formatter");
  need(f.isAutoMemoryFile,"auto memory file classification");
  need(f.diagnosticBeforeEdit,"pre-edit diagnostics");
  need(f.notifyEditor,"editor update notification");
  need(f.historyEnabled,"file history feature");
  need(f.lspAvailable,"LSP availability");
  need(f.remoteDiffEnabled,"remote diff feature");
  need(f.checkTeamMemorySecrets,"team memory secret check");
  need(f.validateSettingsEdit,"settings file edit validation");
  need(f.localeCompare,"locale comparator");need(f.nowMs,"clock");
  need(f.media.getGadgetResultsDir,"media results directory");
  need(f.media.randomUUID,"media UUID");
  need(f.media.isPDFSupported,"model PDF capability");
}
}

IndigenousFileServices makeIndigenousFileServices(const Inputs& input){
  const auto& f=input.files;requireFiles(f);
  if(!input.messages)throw std::invalid_argument("Application message services required");
  IndigenousFileServices out;
  const auto path=paths(input);
  const auto readStash=f.editReadStash?f.editReadStash:
    std::make_shared<IndigenousFileReadStash>();
  const auto similar=f.findSimilarFile?f.findSimilarFile:findSimilarFile;
  const auto suggest=f.suggestUnderCwd?f.suggestUnderCwd:
    std::function<std::optional<std::string>(const std::string&)>(
      [cwd=input.cwd](const std::string& requested){
        return suggestPathUnderCwd(requested,cwd());});
  const auto roster=f.skillRoster;
  const auto skillServices=f.skillServices;
  const auto onOwner=f.runOnOwnerAndWait;
  auto discover=[roster,skillServices,onOwner](const std::string& file,
      const std::string& cwd){
    std::vector<std::string> found;
    onOwner([&]{found=skills::discoverSkillDirsForPaths({file},cwd,*roster,skillServices);});
    return found;
  };
  auto add=[roster,skillServices,onOwner](const std::vector<std::string>& dirs){
    onOwner([&]{skills::addSkillDirectories(dirs,*roster,skillServices);});
  };
  auto activate=[roster,skillServices,onOwner](const std::string& file,
      const std::string& cwd){
    onOwner([&]{(void)skills::activateConditionalSkillsForPaths(
      {file},cwd,*roster,skillServices);});
  };
  auto editBase=[&](gadgets::files::FileWriteServices& s){
    s.paths=path;s.readFileStateOwner=f.stash;s.readFileState=f.stash.get();
    s.matchingEditDenyRule=f.editDenied;
    s.discoverSkillDirsForPaths=discover;s.addSkillDirectories=add;
    s.activateConditionalSkillsForPaths=activate;
    s.checkTeamMemSecrets=f.checkTeamMemorySecrets;
    s.diagnosticBeforeFileEdited=f.diagnosticBeforeEdit;
    s.fileHistoryEnabled=f.historyEnabled;s.fileHistoryTrackEdit=f.trackHistory;
    s.getPatchForDisplay=f.patch;s.notifyEditorFileUpdated=f.notifyEditor;
    s.lspAvailable=f.lspAvailable;s.clearDeliveredDiagnostics=f.clearDiagnostics;
    s.lspChangeFile=f.lspChange;s.lspSaveFile=f.lspSave;
    s.remoteDiffEnabled=f.remoteDiffEnabled;s.fetchSingleFileGitDiff=f.remoteDiff;
    if(f.historyEnabled())need(f.trackHistory,"file history tracker");
    if(f.lspAvailable()){
      need(f.clearDiagnostics,"LSP clear diagnostics");
      need(f.lspChange,"LSP file change");need(f.lspSave,"LSP file save");
    }
    if(f.remoteDiffEnabled())need(f.remoteDiff,"remote diff fetch");
  };
  editBase(out.write);editBase(out.edit);
  out.edit.editUtils.paths=path;
  out.edit.editUtils.getPatchForDisplay=f.patch;
  out.edit.editUtils.getPatchFromContents=f.patch;
  out.edit.editUtils.readFileSyncStashed=[path,readStash](const std::string& file){
    return readStash->read(filesystem::expandPath(file,path));};
  out.edit.simpleMode=[simple=f.simpleMode]{return simple;};
  out.edit.findSimilarFile=similar;
  out.edit.suggestPathUnderCwd=suggest;
  out.edit.formatFileSize=formatting::formatFileSize;
  out.edit.compactLinePrefixEnabled=[value=f.compactLinePrefix]{return value;};
  out.edit.conciseEditHint=[value=f.conciseEditHint]{return value;};
  out.edit.validateInputForSettingsFileEdit=f.validateSettingsEdit;

  auto& r=out.read;
  r.paths=path;r.readFileStateOwner=f.stash;r.readFileState=f.stash.get();
  r.messageServices=input.messages.get();r.defaultLimits=f.readLimits;
  r.range.formatFileSize=formatting::formatFileSize;
  r.dedupKillswitch=[value=f.dedupDisabled]{return value;};
  r.simpleMode=[value=f.simpleMode]{return value;};
  r.matchingReadDenyRule=f.readDenied;
  r.discoverSkillDirsForPaths=discover;r.addSkillDirectories=add;
  r.activateConditionalSkillsForPaths=activate;
  r.findSimilarFile=similar;r.suggestPathUnderCwd=suggest;
  r.roughTokenCountEstimationForFileType=roughTokenCountEstimationForFileType;
  r.countTokensWithAPI=f.countTokens;
  gadgets::files::notebook::Services notebook;
  notebook.paths=path;notebook.formatOutput=f.notebookFormatOutput;
  r.readNotebook=[notebook](const std::string& file){
    return gadgets::files::notebook::readNotebook(file,notebook);};
  r.mapNotebookCellsToGadgetResult=gadgets::files::notebook::mapNotebookCellsToGadgetResult;
  media::bindIndigenousReadMedia(r,f.media);
  r.isAutoMemFile=f.isAutoMemoryFile;
  r.memoryFreshnessNote=[clock=f.nowMs](double mtime){
    return instructions::memoryFreshnessNote(mtime,clock());};
  r.shouldIncludeFileReadMitigation=[value=f.includeReadMitigation]{return value;};
  r.compactLinePrefixEnabled=[value=f.compactLinePrefix]{return value;};

  out.glob.paths=path;out.grep.paths=path;
  out.glob.suggestPathUnderCwd=suggest;
  out.grep.suggestPathUnderCwd=suggest;
  out.glob.getFileReadIgnorePatterns=f.readIgnorePatterns;
  out.grep.getFileReadIgnorePatterns=f.readIgnorePatterns;
  out.glob.normalizePatternsToPath=f.normalizeIgnorePatterns;
  out.grep.normalizePatternsToPath=f.normalizeIgnorePatterns;
  out.glob.getGlobExclusionsForPluginStash=f.pluginGlobExclusions;
  out.grep.getGlobExclusionsForPluginStash=f.pluginGlobExclusions;
  const auto ripgrep=f.ripgrep;
  auto runRipgrep=[ripgrep](const std::vector<std::string>& args,
      const std::string& target,std::stop_token stop){
    return filesystem::ripGrep(args,target,stop,ripgrep);};
  out.glob.ripGrep=runRipgrep;out.grep.ripGrep=runRipgrep;
  out.glob.nowMs=f.nowMs;
  out.glob.noIgnore=[value=f.noIgnore]{return value;};
  out.glob.hidden=[value=f.hidden]{return value;};
  out.glob.platform=path.platform;
  out.grep.localeCompare=f.localeCompare;
  return out;
}
} // namespace batbox::application_gadgets
