#include "settings/mdm/Parse.hpp"
#include "filesystem/FileOps.hpp"
#include <filesystem>
#include <utility>

namespace batbox::settings::mdm {
ParseServices indigenousParseServices(Paths& paths,
    persistence::SafeJsonParser& safeJson,settings::ParseServices parser,
    Names names){
  ParseServices services;
  services.safeJson=&safeJson;
  services.parser=std::move(parser);
  services.paths=&paths;
  services.names=std::move(names);
  services.readFileSync=[](const std::string& path){
    return filesystem::readFileSyncWithMetadata(path).content;
  };
  services.readdirSync=[](const std::string& directory){
    std::vector<DirEntry> result;
    for(const auto& item:std::filesystem::directory_iterator(directory)){
      const auto status=item.symlink_status();
      result.push_back({item.path().filename().string(),
        std::filesystem::is_regular_file(status),
        std::filesystem::is_symlink(status)});
    }
    return result;
  };
  return services;
}
}
