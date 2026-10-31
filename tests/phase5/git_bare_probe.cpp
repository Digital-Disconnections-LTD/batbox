#include "git/BareRepository.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json output=Json::array();
  for(const auto& item:value::parse(data)){
    Json logs=Json::array();git::BareRepositoryImports imports;
    imports.getCwd=[&]{logs.push_back("getCwd");return item.at("cwd").get<std::string>();};
    imports.statSync=[&](const std::string& path){
      logs.push_back("stat:"+path);
      const auto& stats=item.at("stats");
      if(!stats.contains(path))throw std::runtime_error("ENOENT");
      const auto kind=stats.at(path).get<std::string>();
      return git::BareStat{kind=="file",kind=="directory"};
    };
    output.push_back({{"name",item.at("name")},
      {"value",git::isCurrentDirectoryBareGitRepo(imports)},{"logs",logs}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
