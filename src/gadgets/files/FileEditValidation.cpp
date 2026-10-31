#include "gadgets/files/FileEdit.hpp"
#include "formatting/Format.hpp"
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::gadgets::files {
namespace {
constexpr std::uintmax_t maxEditFileSize=1024ull*1024*1024;
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing FileEdit binding: ")+name);
}
ValidationResult ask(const std::string& message,int code,const Json& meta=Json()){
  ValidationResult result{false,message,code};result.behavior="ask";result.meta=meta;return result;
}
bool containsUTF16(const std::string& file,const std::string& needle){
  return value::utf16CodeUnits(file).find(value::utf16CodeUnits(needle))!=std::u16string::npos;
}
std::size_t countMatches(const std::string& file,const std::string& needle){
  const auto text=value::utf16CodeUnits(file),search=value::utf16CodeUnits(needle);
  if(search.empty())return text.size()+1;
  std::size_t count=0,pos=0;
  while((pos=text.find(search,pos))!=std::u16string::npos){++count;pos+=search.size();}
  return count;
}
} // namespace

ValidationResult validateEditInput(const Json& input,const FileEditServices& services){
  const auto path=input.at("file_path").get<std::string>();
  const auto oldString=input.at("old_string").get<std::string>();
  const auto newString=input.at("new_string").get<std::string>();
  const bool replaceAll=input.value("replace_all",false);
  const auto full=filesystem::expandPath(path,services.paths);
  require(static_cast<bool>(services.checkTeamMemSecrets),"checkTeamMemSecrets");
  if(const auto secret=services.checkTeamMemSecrets(full,newString))
    return ValidationResult{false,*secret,0};
  if(oldString==newString)return ask("No changes to make: old_string and new_string are exactly the same.",1);
  require(static_cast<bool>(services.matchingEditDenyRule),"matchingEditDenyRule");
  if(services.matchingEditDenyRule(full))
    return ask("File is in a directory that is denied by your permission settings.",2);
  if(full.starts_with("\\\\")||full.starts_with("//"))return ValidationResult{true};
  std::error_code ec;
  const auto size=std::filesystem::file_size(full,ec);
  if(!ec&&size>maxEditFileSize){
    const auto format=services.formatFileSize?services.formatFileSize:formatting::formatFileSize;
    return ask("File is too large to edit ("+format(size)+"). Maximum editable file size is "+
      format(maxEditFileSize)+".",10);
  }
  if(ec&&ec!=std::errc::no_such_file_or_directory)
    throw std::filesystem::filesystem_error("stat",full,ec);
  std::optional<std::string> fileContent;
  try{fileContent=filesystem::readFileSyncWithMetadata(full).content;}
  catch(const std::system_error& e){if(e.code()!=std::errc::no_such_file_or_directory)throw;}
  if(!fileContent){
    if(oldString.empty())return ValidationResult{true};
    require(static_cast<bool>(services.findSimilarFile),"findSimilarFile");
    require(static_cast<bool>(services.suggestPathUnderCwd),"suggestPathUnderCwd");
    const auto similar=services.findSimilarFile(full);
    const auto suggestion=services.suggestPathUnderCwd(full);
    const auto cwd=services.paths.getCwd?services.paths.getCwd():std::filesystem::current_path().string();
    std::string message="File does not exist. "+services.fileNotFoundCwdNote+" "+cwd+".";
    if(suggestion)message+=" Did you mean "+*suggestion+"?";
    else if(!similar.empty())message+=" Did you mean "+similar+"?";
    return ask(message,4);
  }
  if(oldString.empty()){
    if(!value::trim(*fileContent).empty())return ask("Cannot create new file - file already exists.",3);
    return ValidationResult{true};
  }
  if(full.ends_with(".ipynb"))
    return ask("File is a Jupyter Notebook. Use the "+services.notebookEditGadgetName+" to edit this file.",5);
  require(services.readFileState!=nullptr,"readFileState");
  const auto read=services.readFileState->get(full);
  if(!read||read->isPartialView)
    return ask("File has not been read yet. Read it first before writing to it.",6,
      Json{{"isFilePathAbsolute",std::filesystem::path(path).is_absolute()?"true":"false"}});
  const auto mtime=filesystem::getFileModificationTime(full);
  if(mtime>read->timestamp){
    const bool fullRead=!read->offset&&!read->limit;
    if(!(fullRead&&*fileContent==read->content))
      return ask("File has been modified since read, either by the user or by a linter. Read it again before attempting to write it.",7);
  }
  const auto actual=edit::findActualString(*fileContent,oldString);
  if(!actual||actual->empty())
    return ask("String to replace not found in file.\nString: "+oldString,8,
      Json{{"isFilePathAbsolute",std::filesystem::path(path).is_absolute()?"true":"false"}});
  const auto matches=countMatches(*fileContent,*actual);
  if(matches>1&&!replaceAll)
    return ask("Found "+std::to_string(matches)+
      " matches of the string to replace, but replace_all is false. To replace all occurrences, set replace_all to true. To replace only one occurrence, please provide more context to uniquely identify the instance.\nString: "+oldString,9,
      Json{{"isFilePathAbsolute",std::filesystem::path(path).is_absolute()?"true":"false"},
        {"actualOldString",*actual}});
  require(static_cast<bool>(services.validateInputForSettingsFileEdit),"validateInputForSettingsFileEdit");
  const auto settings=services.validateInputForSettingsFileEdit(full,*fileContent,[&](){
    return edit::applyEditToFile(*fileContent,*actual,newString,replaceAll);});
  if(settings)return *settings;
  ValidationResult success{true};success.meta=Json{{"actualOldString",*actual}};return success;
}
} // namespace batbox::gadgets::files
