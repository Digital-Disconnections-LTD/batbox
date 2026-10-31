#include "settings/Loader.hpp"
#include "settings/Merge.hpp"
#include <filesystem>
#include <set>

namespace batbox::settings {
namespace {
bool present(const Json& value){
  if(value.is_object()||value.is_array())return !value.empty();
  if(value.is_string())return !value.get_ref<const std::string&>().empty();
  return false;
}
std::string keyPart(const Json& value,const char* key){
  const auto& part=value::property(value,key);
  return part.is_null()?"undefined":part.is_string()
    ?part.get<std::string>():value::stringify(part);
}
std::string errorKey(const Json& error){
  return keyPart(error,"file")+":"+keyPart(error,"path")+":"+
    keyPart(error,"message");
}
void addErrors(Json& target,std::set<std::string>& seen,const Json& errors){
  for(const auto& error:errors)if(seen.insert(errorKey(error)).second)
    target.push_back(error);
}
}
ParsedSettings Loader::loadSettingsFromDisk(){
  if(loading_)return {Json::object(),Json::array()};
  loading_=true;
  try{
    auto merged=Json::object();
    if(const auto plugin=services_.stash->getPluginSettingsBase())
      merged=mergeSettings(merged,*plugin);
    Json allErrors=Json::array();
    std::set<std::string> seenErrors,seenFiles;
    for(const auto source:getEnabledSettingSources(services_.allowedSources)){
      if(source==Source::Policy){
        Json policy=nullptr,policyErrors=Json::array();
        if(!services_.remoteManagedSettings)
          throw std::invalid_argument("Missing settings load binding: remoteManagedSettings");
        const auto remote=services_.remoteManagedSettings();
        if(remote&&present(*remote)){
          if(!services_.parser.validateSettingsSchema)
            throw std::invalid_argument("Missing settings schema binding");
          const auto parsed=services_.parser.validateSettingsSchema(*remote);
          if(parsed.success)policy=parsed.data;
          else{
            if(!services_.parser.formatZodError)
              throw std::invalid_argument("Missing settings error formatter binding");
            const auto errors=services_.parser.formatZodError(parsed.issues,
              "remote managed settings");
            for(const auto& error:errors)policyErrors.push_back(error);
          }
        }
        if(policy.is_null()){
          if(!services_.getMdmSettings)
            throw std::invalid_argument("Missing settings load binding: getMdmSettings");
          const auto mdm=services_.getMdmSettings();
          if(present(mdm.settings))policy=mdm.settings;
          for(const auto& error:mdm.errors)policyErrors.push_back(error);
        }
        if(policy.is_null()){
          const auto file=loadManagedFileSettings(*services_.paths,
            *services_.stash,services_.parser);
          if(!file.settings.is_null())policy=file.settings;
          for(const auto& error:file.errors)policyErrors.push_back(error);
        }
        if(policy.is_null()){
          if(!services_.getHkcuSettings)
            throw std::invalid_argument("Missing settings load binding: getHkcuSettings");
          const auto hkcu=services_.getHkcuSettings();
          if(present(hkcu.settings))policy=hkcu.settings;
          for(const auto& error:hkcu.errors)policyErrors.push_back(error);
        }
        if(!policy.is_null())merged=mergeSettings(merged,policy);
        addErrors(allErrors,seenErrors,policyErrors);
        continue;
      }
      const auto path=services_.paths->getSettingsFilePathForSource(source);
      if(path){
        const auto resolved=std::filesystem::absolute(*path)
          .lexically_normal().string();
        if(seenFiles.insert(resolved).second){
          const auto file=parseSettingsFile(*path,*services_.stash,
            services_.parser);
          addErrors(allErrors,seenErrors,file.errors);
          if(!file.settings.is_null())
            merged=mergeSettings(merged,file.settings);
        }
      }
      if(source==Source::Flag){
        if(!services_.flagSettingsInline)
          throw std::invalid_argument("Missing settings load binding: flagSettingsInline");
        const auto inlineSettings=services_.flagSettingsInline();
        if(inlineSettings){
          if(!services_.parser.validateSettingsSchema)
            throw std::invalid_argument("Missing settings schema binding");
          const auto parsed=services_.parser.validateSettingsSchema(*inlineSettings);
          if(parsed.success)merged=mergeSettings(merged,parsed.data);
        }
      }
    }
    loading_=false;
    return {merged,allErrors};
  }catch(...){loading_=false;throw;}
}
ParsedSettings Loader::getSettingsWithErrors(){
  if(const auto stashed=services_.stash->getFlightSettingsStash())
    return {value::property(*stashed,"settings"),value::property(*stashed,"errors")};
  const auto result=loadSettingsFromDisk();
  services_.stash->setFlightSettingsStash(
    {{"settings",result.settings},{"errors",result.errors}});
  return result;
}
Json Loader::getInitialSettings(){
  const auto settings=getSettingsWithErrors().settings;
  return settings.is_null()?Json::object():settings;
}
SettingsWithSources Loader::getSettingsWithSources(){
  services_.stash->resetSettingsStash();
  Json sources=Json::array();
  for(const auto source:getEnabledSettingSources(services_.allowedSources)){
    const auto settings=getSettingsForSource(source);
    if(present(settings))sources.push_back(
      {{"source",sourceKey(source)},{"settings",settings}});
  }
  return {getInitialSettings(),sources};
}
}
