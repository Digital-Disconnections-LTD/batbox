#include "git/CanonicalRoot.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);const std::string bytes(std::istreambuf_iterator<char>{stream},{});
  Json output=Json::array();
  for(const auto& item:value::parse(bytes)){
    Json logs=Json::array();git::CanonicalRootImports imports;
    imports.findGitRoot=[&](const std::string& start)->std::optional<std::string>{
      logs.push_back("find:"+start);
      return item.at("root").is_null()?std::nullopt:
        std::optional<std::string>(item.at("root").get<std::string>());
    };
    imports.readFileSync=[&](const std::string& path){
      logs.push_back("read:"+path);
      const auto& files=item.at("files");
      if(!files.contains(path))throw std::runtime_error("ENOENT");
      return files.at(path).get<std::string>();
    };
    imports.realpathSync=[&](const std::string& path){
      logs.push_back("realpath:"+path);
      const auto& paths=item.at("realpaths");
      if(!paths.contains(path))throw std::runtime_error("ENOENT");
      return paths.at(path).get<std::string>();
    };
    git::CanonicalRoot canonical(imports);
    auto first=canonical.find(item.at("start").get<std::string>());
    auto second=canonical.find(item.at("start").get<std::string>());
    output.push_back({{"name",item.at("name")},{"first",first?Json(*first):Json(nullptr)},
      {"second",second?Json(*second):Json(nullptr)},
      {"cacheSize",canonical.stashSize()},{"logs",logs}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
