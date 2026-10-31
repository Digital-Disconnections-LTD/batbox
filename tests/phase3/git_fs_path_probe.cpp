#include "git_filesystem/GitFilesystemInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream file(argv[1]);
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>{file},{}));
  Json results=Json::array();for(const auto& item:cases){
    if(item.size()==1)results.push_back(git_filesystem::resolvePath(item[0].get<std::string>()));
    else results.push_back(git_filesystem::resolvePath(item[0].get<std::string>(),
      item[1].get<std::string>()));
  }
  std::cout<<value::stringify(results)<<'\n';
}
