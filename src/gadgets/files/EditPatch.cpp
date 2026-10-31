#include "gadgets/files/EditUtils.hpp"
#include <stdexcept>

namespace batbox::gadgets::files::edit {
namespace {
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing FileEdit binding: ")+name);
}
bool containsUTF16(const std::string& text,const std::string& needle){
  return value::utf16CodeUnits(text).find(value::utf16CodeUnits(needle))!=std::u16string::npos;
}
std::string stripNewlineSuffix(std::string text){while(!text.empty()&&text.back()=='\n')text.pop_back();return text;}
} // namespace

Json getPatchForEdits(const std::string& filePath,const std::string& fileContents,
                      const Json& edits,const Services& services){
  std::string updatedFile=fileContents;
  std::vector<std::string> appliedNewStrings;
  if(fileContents.empty()&&edits.size()==1&&edits.at(0).at("old_string")==""&&
     edits.at(0).at("new_string")==""){
    require(static_cast<bool>(services.getPatchForDisplay),"getPatchForDisplay");
    return Json{{"patch",services.getPatchForDisplay(filePath,fileContents,updatedFile)},
      {"updatedFile",""}};
  }
  for(const auto& edit:edits){
    const auto oldString=edit.at("old_string").get<std::string>();
    const auto newString=edit.at("new_string").get<std::string>();
    const auto oldToCheck=stripNewlineSuffix(oldString);
    for(const auto& previous:appliedNewStrings)
      if(!oldToCheck.empty()&&containsUTF16(previous,oldToCheck))
        throw std::runtime_error("Cannot edit file: old_string is a substring of a new_string from a previous edit.");
    const auto previousContent=updatedFile;
    updatedFile=oldString.empty()?newString:applyEditToFile(updatedFile,oldString,newString,
      edit.value("replace_all",false));
    if(updatedFile==previousContent)
      throw std::runtime_error("String not found in file. Failed to apply edit.");
    appliedNewStrings.push_back(newString);
  }
  if(updatedFile==fileContents)
    throw std::runtime_error("Original and edited file match exactly. Failed to apply edit.");
  require(static_cast<bool>(services.getPatchFromContents),"getPatchFromContents");
  const auto patch=services.getPatchFromContents(filePath,
    convertLeadingTabsToSpaces(fileContents),convertLeadingTabsToSpaces(updatedFile));
  return Json{{"patch",patch},{"updatedFile",updatedFile}};
}

Json getPatchForEdit(const std::string& filePath,const std::string& fileContents,
                     const std::string& oldString,const std::string& newString,
                     bool replaceAll,const Services& services){
  return getPatchForEdits(filePath,fileContents,
    Json::array({Json{{"old_string",oldString},{"new_string",newString},{"replace_all",replaceAll}}}),services);
}

bool areFileEditsEquivalent(const Json& edits1,const Json& edits2,
                            const std::string& originalContent,const Services& services){
  if(edits1==edits2)return true;
  std::optional<std::string> result1,result2,error1,error2;
  try{result1=getPatchForEdits("temp",originalContent,edits1,services).at("updatedFile").get<std::string>();}
  catch(const std::exception& e){error1=e.what();}
  try{result2=getPatchForEdits("temp",originalContent,edits2,services).at("updatedFile").get<std::string>();}
  catch(const std::exception& e){error2=e.what();}
  if(error1&&error2)return *error1==*error2;
  if(error1||error2)return false;
  return result1==result2;
}

bool areFileEditsInputsEquivalent(const Json& input1,const Json& input2,const Services& services){
  const auto path1=input1.at("file_path").get<std::string>();
  if(path1!=input2.at("file_path").get<std::string>())return false;
  const auto& edits1=input1.at("edits"),&edits2=input2.at("edits");
  if(edits1==edits2)return true;
  require(static_cast<bool>(services.readFileSyncStashed),"readFileSyncCached");
  std::string original;
  try{original=services.readFileSyncStashed(path1);}
  catch(const std::system_error& e){if(e.code()!=std::errc::no_such_file_or_directory)throw;}
  return areFileEditsEquivalent(edits1,edits2,original,services);
}
} // namespace batbox::gadgets::files::edit
