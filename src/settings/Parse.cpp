#include "settings/Parse.hpp"
#include "filesystem/FileOps.hpp"
#include <stdexcept>
#include <system_error>

namespace batbox::settings {
namespace {
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing settings parser binding: ")+name);
}
Json concatErrors(const Json& left,const Json& right){
  Json result=left;
  for(const auto& item:right)result.push_back(item);
  return result;
}
void handleFileSystemError(const std::exception& error,
  const std::string& path,const ParseServices& services){
  if(const auto* osError=dynamic_cast<const std::system_error*>(&error))
    if(osError->code()==std::errc::no_such_file_or_directory)return;
  if(services.logFileError)services.logFileError(path,error.what());
}
ParsedSettings parseSettingsFileUncached(const std::string& path,
  const ParseServices& services){
  need(services.filterInvalidPermissionRules,"filterInvalidPermissionRules");
  need(services.validateSettingsSchema,"validateSettingsSchema");
  need(services.formatZodError,"formatZodError");
  try{
    const auto resolved=filesystem::safeResolvePath(path);
    const auto content=filesystem::readFileSyncWithMetadata(
      resolved.resolvedPath).content;
    if(value::trim(content).empty())return {Json::object(),Json::array()};
    const auto jsonText=content.starts_with("\xef\xbb\xbf")
      ?content.substr(3):content;
    auto data=value::parse(jsonText,false);
    if(data.is_discarded())data=nullptr;
    auto warnings=services.filterInvalidPermissionRules(data,path);
    const auto validation=services.validateSettingsSchema(data);
    if(!validation.success){
      return {nullptr,concatErrors(warnings,
        services.formatZodError(validation.issues,path))};
    }
    return {validation.data,warnings};
  }catch(const std::exception& error){
    handleFileSystemError(error,path,services);
    return {nullptr,Json::array()};
  }
}
}
ParsedSettings parseSettingsFile(const std::string& path,
  Stash& stash,const ParseServices& services){
  if(const auto stashed=stash.getStashedParsedFile(path))return *stashed;
  const auto result=parseSettingsFileUncached(path,services);
  stash.setStashedParsedFile(path,result);
  return result;
}
}
