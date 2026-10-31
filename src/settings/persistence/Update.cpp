#include "settings/persistence/Update.hpp"
#include "filesystem/FileOps.hpp"
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::settings::persistence {
namespace {
bool isMissingFile(const std::exception& error){
  const auto* system=dynamic_cast<const std::system_error*>(&error);
  return system&&system->code()==std::errc::no_such_file_or_directory;
}
void debug(const UpdateServices& services,const std::string& message){
  if(services.debug)services.debug(message);
}
}
UpdateResult updateSettingsForSource(Source source,const UpdatePatch& patch,
  const UpdateServices& services){
  if(source==Source::Policy||source==Source::Flag)return {};
  if(!services.paths||!services.stash||!services.internalWrites||
      !services.safeJsonParser)
    throw std::invalid_argument("Missing settings update services");
  if(source==Source::Local&&!services.scheduleGitignore)
    throw std::invalid_argument("Missing settings gitignore scheduler");
  const auto filePath=services.paths->getSettingsFilePathForSource(source);
  if(!filePath)return {};
  try{
    std::filesystem::create_directories(
      std::filesystem::path(*filePath).parent_path());

    // Use a separate stash so a failed write cannot mutate stashed state.
    Stash scratch;
    const auto validated=parseSettingsFile(*filePath,scratch,services.parser);
    Json existing=validated.settings;
    if(existing.is_null()){
      std::optional<std::string> content;
      try{content=filesystem::readFileSyncWithMetadata(*filePath).content;}
      catch(const std::exception& error){
        if(!isMissingFile(error))throw;
      }
      if(content){
        const auto raw=services.safeJsonParser->safeParseJSON(content);
        if(raw.is_null())
          return {"Invalid JSON syntax in settings file at "+*filePath};
        if(raw.is_object()||raw.is_array()){
          existing=raw;
          debug(services,"Using raw settings from "+*filePath+
            " due to validation failure");
        }
      }
    }

    const auto updated=mergeForUpdate(existing.is_null()?
      Json::object():existing,patch);
    services.internalWrites->markInternalWrite(*filePath);
    filesystem::writeTextContent(*filePath,
      value::stringify(updated,2)+"\n","utf8","LF");
    services.stash->resetSettingsStash();
    if(source==Source::Local){
      services.scheduleGitignore(
        services.paths->getRelativeSettingsFilePathForSource(Source::Local),
        services.originalCwd);
    }
    return {};
  }catch(const std::exception& error){
    const auto message="Failed to read raw settings from "+*filePath+
      ": "+error.what();
    if(services.logError)services.logError(message);
    return {message};
  }
}
}
