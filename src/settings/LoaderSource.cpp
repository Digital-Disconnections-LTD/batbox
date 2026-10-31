#include "settings/Loader.hpp"
#include "settings/Merge.hpp"
#include <stdexcept>

namespace batbox::settings {
namespace {
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing settings load binding: ")+name);
}
bool present(const Json& value){
  if(value.is_object()||value.is_array())return !value.empty();
  if(value.is_string())return !value.get_ref<const std::string&>().empty();
  return false;
}
}
Loader::Loader(LoadServices services):services_(std::move(services)){
  if(!services_.paths||!services_.stash)
    throw std::invalid_argument("Missing settings paths/cache binding");
}
Json Loader::getSettingsForSource(Source source){
  if(const auto stashed=services_.stash->getStashedSettingsForSource(source))
    return *stashed;
  auto result=getSettingsForSourceUncached(source);
  services_.stash->setStashedSettingsForSource(source,result);
  return result;
}
Json Loader::getSettingsForSourceUncached(Source source){
  if(source==Source::Policy){
    need(services_.remoteManagedSettings,"remoteManagedSettings");
    const auto remote=services_.remoteManagedSettings();
    if(remote&&present(*remote))return *remote;
    need(services_.getMdmSettings,"getMdmSettings");
    const auto mdm=services_.getMdmSettings();
    if(present(mdm.settings))return mdm.settings;
    const auto file=loadManagedFileSettings(*services_.paths,*services_.stash,
      services_.parser);
    if(!file.settings.is_null())return file.settings;
    need(services_.getHkcuSettings,"getHkcuSettings");
    const auto hkcu=services_.getHkcuSettings();
    if(present(hkcu.settings))return hkcu.settings;
    return nullptr;
  }
  const auto path=services_.paths->getSettingsFilePathForSource(source);
  auto file=path?parseSettingsFile(*path,*services_.stash,services_.parser)
    :ParsedSettings{};
  if(source==Source::Flag){
    need(services_.flagSettingsInline,"flagSettingsInline");
    const auto inlineSettings=services_.flagSettingsInline();
    if(inlineSettings){
      need(services_.parser.validateSettingsSchema,"validateSettingsSchema");
      const auto parsed=services_.parser.validateSettingsSchema(*inlineSettings);
      if(parsed.success)return mergeSettings(file.settings.is_null()
        ?Json::object():file.settings,parsed.data);
    }
  }
  return file.settings;
}
std::optional<std::string> Loader::getPolicySettingsOrigin(){
  need(services_.remoteManagedSettings,"remoteManagedSettings");
  const auto remote=services_.remoteManagedSettings();
  if(remote&&present(*remote))return "remote";
  need(services_.getMdmSettings,"getMdmSettings");
  if(present(services_.getMdmSettings().settings))
    return services_.platform=="macos"?"plist":"hklm";
  if(!loadManagedFileSettings(*services_.paths,*services_.stash,
     services_.parser).settings.is_null())return "file";
  need(services_.getHkcuSettings,"getHkcuSettings");
  if(present(services_.getHkcuSettings().settings))return "hkcu";
  return std::nullopt;
}
}
