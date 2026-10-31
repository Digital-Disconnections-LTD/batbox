#include "gadgets/files/FileEdit.hpp"
#include "gadgets/files/ActiveTriggers.hpp"
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::gadgets::files {
namespace {
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing FileEdit binding: ")+name);
}
struct ExistingFile{std::string content;bool exists=false;std::string encoding="utf8";std::string endings="LF";};
ExistingFile readFileForEdit(const std::string& path){
  try{
    const auto meta=filesystem::readFileSyncWithMetadata(path);
    return {meta.content,true,meta.encoding,meta.lineEndings};
  }catch(const std::system_error& e){
    if(e.code()!=std::errc::no_such_file_or_directory)throw;
    return {};
  }
}
} // namespace

GadgetResult callEdit(const Json& input,const std::string& parentMessageUUID,
                    const GadgetCallContext& context,FileEditServices& services){
  const auto path=input.at("file_path").get<std::string>();
  const auto oldString=input.at("old_string").get<std::string>();
  const auto newString=input.at("new_string").get<std::string>();
  const bool replaceAll=input.value("replace_all",false);
  const auto full=filesystem::expandPath(path,services.paths);
  require(static_cast<bool>(services.paths.getCwd),"getCwd");
  const auto cwd=services.paths.getCwd();
  require(static_cast<bool>(services.simpleMode),"simpleMode");
  if(!services.simpleMode()){
    require(static_cast<bool>(services.discoverSkillDirsForPaths),"discoverSkillDirsForPaths");
    const auto dirs=services.discoverSkillDirsForPaths(full,cwd);
    if(!dirs.empty()){
      for(const auto& dir:dirs)recordDynamicSkillDir(services,dir);
      require(static_cast<bool>(services.addSkillDirectories),"addSkillDirectories");
      services.addSkillDirectories(dirs);
    }
    require(static_cast<bool>(services.activateConditionalSkillsForPaths),"activateConditionalSkillsForPaths");
    services.activateConditionalSkillsForPaths(full,cwd);
  }
  require(static_cast<bool>(services.diagnosticBeforeFileEdited),"diagnosticBeforeFileEdited");
  services.diagnosticBeforeFileEdited(full);
  std::filesystem::create_directories(std::filesystem::path(full).parent_path());
  require(static_cast<bool>(services.fileHistoryEnabled),"fileHistoryEnabled");
  if(services.fileHistoryEnabled()){
    require(static_cast<bool>(services.fileHistoryTrackEdit),"fileHistoryTrackEdit");
    services.fileHistoryTrackEdit(full,parentMessageUUID);
  }
  const auto file=readFileForEdit(full);
  require(services.readFileState!=nullptr,"readFileState");
  if(file.exists){
    const auto lastWrite=filesystem::getFileModificationTime(full);
    const auto prior=services.readFileState->get(full);
    if(!prior||lastWrite>prior->timestamp){
      const bool fullRead=prior&&!prior->offset&&!prior->limit;
      if(!(fullRead&&file.content==prior->content))
        throw std::runtime_error(services.unexpectedModifiedError);
    }
  }
  const auto found=edit::findActualString(file.content,oldString);
  const auto actualOld=found&&!found->empty()?*found:oldString;
  const auto actualNew=edit::preserveQuoteStyle(oldString,actualOld,newString);
  auto editServices=services.editUtils;
  editServices.paths=services.paths;
  const auto patchResult=edit::getPatchForEdit(full,file.content,actualOld,actualNew,replaceAll,editServices);
  const auto& patch=patchResult.at("patch");
  const auto updated=patchResult.at("updatedFile").get<std::string>();
  filesystem::writeTextContent(full,updated,file.encoding,file.endings);
  require(static_cast<bool>(services.lspAvailable),"lspAvailable");
  if(services.lspAvailable()){
    require(static_cast<bool>(services.clearDeliveredDiagnostics),"clearDeliveredDiagnostics");
    require(static_cast<bool>(services.lspChangeFile),"lspChangeFile");
    require(static_cast<bool>(services.lspSaveFile),"lspSaveFile");
    services.clearDeliveredDiagnostics("file://"+full);
    try{services.lspChangeFile(full,updated);}catch(const std::exception&){}
    try{services.lspSaveFile(full);}catch(const std::exception&){}
  }
  require(static_cast<bool>(services.notifyEditorFileUpdated),"notifyEditorFileUpdated");
  services.notifyEditorFileUpdated(full,file.content,updated);
  services.readFileState->set(full,ReadSnapshot{updated,filesystem::getFileModificationTime(full),false,
    std::nullopt,std::nullopt});
  Json diff;
  require(static_cast<bool>(services.remoteDiffEnabled),"remoteDiffEnabled");
  if(services.remoteDiffEnabled()){
    require(static_cast<bool>(services.fetchSingleFileGitDiff),"fetchSingleFileGitDiff");
    if(const auto value=services.fetchSingleFileGitDiff(full))diff=*value;
  }
  Json data={{"filePath",path},{"oldString",actualOld},{"newString",newString},
    {"originalFile",file.content},{"structuredPatch",patch},
    {"userModified",context.data.is_object()?context.data.value("userModified",false):false},
    {"replaceAll",replaceAll}};
  if(!diff.is_null())data["gitDiff"]=diff;
  GadgetResult result;result.data=std::move(data);return result;
}
} // namespace batbox::gadgets::files
