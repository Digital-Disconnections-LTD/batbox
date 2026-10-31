#include "git_filesystem/RootFinder.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream file(argv[1]);
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>{file},{}));
  Json output=Json::array();for(const auto& item:cases){
    const auto paths=item.value("gitPaths",Json::array()).get<std::set<std::string>>();
    Json logs=Json::array(),values=Json::array();
    git_filesystem::RootFinder finder([&](const std::string& path){
      logs.push_back(path);
      if(!paths.contains(path))throw std::runtime_error("ENOENT");
      return git_filesystem::FileStat{false,true};
    });
    for(const auto& start:item.at("starts")){
      const auto value=finder.find(start.get<std::string>());
      values.push_back(value?Json(*value):Json());
    }
    output.push_back({{"values",values},{"logs",logs},{"cacheSize",finder.stashSize()}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
