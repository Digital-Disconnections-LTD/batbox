#include "settings/persistence/WatchTargets.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=batbox::value::parse(line);
    batbox::Json calls=batbox::Json::array();
    batbox::settings::persistence::WatchPathServices paths;
    paths.getPathForSource=[&](batbox::settings::Source source)
        ->std::optional<std::string>{
      const auto name=batbox::settings::sourceKey(source);
      calls.push_back({"path",name});
      const auto& values=test.at("paths");
      if(values.contains(name))return values[name].get<std::string>();
      return std::nullopt;
    };
    paths.getManagedDropInDir=[&](){
      calls.push_back({"drop"});
      return test.at("drop").get<std::string>();
    };
    batbox::settings::persistence::WatchStatReader stat=
      [&](const std::string& path)
        ->std::optional<batbox::settings::persistence::WatchStat>{
      calls.push_back({"stat",path});
      if(test.value("nativeStat",false))
        return batbox::settings::persistence::indigenousWatchStatReader()(path);
      const auto& values=test.at("stat");
      if(!values.contains(path))return std::nullopt;
      const auto kind=values[path].get<std::string>();
      return batbox::settings::persistence::WatchStat{
        kind=="file",kind=="dir"};
    };
    batbox::Json value=nullptr;
    const auto op=test.at("op").get<std::string>();
    if(op=="targets"){
      const auto result=batbox::settings::persistence::getWatchTargets(paths,stat);
      value={{"dirs",result.dirs},{"settingsFiles",result.settingsFiles},
        {"dropInDir",result.dropInDir?batbox::Json(*result.dropInDir):
          batbox::Json(nullptr)}};
    }else if(op=="ignored"){
      batbox::settings::persistence::WatchTargets targets;
      targets.settingsFiles=test.at("settingsFiles").get<std::vector<std::string>>();
      if(!test.at("dropInDir").is_null())
        targets.dropInDir=test.at("dropInDir").get<std::string>();
      std::optional<batbox::settings::persistence::WatchStat> info;
      if(!test.at("stats").is_null()){
        const auto kind=test.at("stats").get<std::string>();
        info=batbox::settings::persistence::WatchStat{
          kind=="file",kind=="dir"};
      }
      value=batbox::settings::persistence::ignoreWatchPath(targets,
        test.at("path").get<std::string>(),info);
    }else if(op=="source"){
      const auto result=batbox::settings::persistence::getSourceForPath(
        paths,test.at("path").get<std::string>());
      if(result)value=batbox::settings::sourceKey(*result);
    }else value=batbox::settings::persistence::
      settingSourceToConfigChangeSource(batbox::settings::sourceFromKey(
        test.at("source").get<std::string>()));
    std::cout<<batbox::value::stringify({{"value",value},{"calls",calls}})<<'\n';
  }
}
