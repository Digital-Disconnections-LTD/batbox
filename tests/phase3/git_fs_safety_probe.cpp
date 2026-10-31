#include "git_filesystem/GitFilesystem.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream file(argv[1]);
  const auto inputs=value::parse(std::string(std::istreambuf_iterator<char>{file},{}));
  Json output=Json::array();for(const auto& item:inputs){
    const auto value=item.get<std::string>();
    output.push_back({{"safe",git_filesystem::isSafeRefName(value)},
      {"sha",git_filesystem::isValidGitSha(value)}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
