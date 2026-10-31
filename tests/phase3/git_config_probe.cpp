#include "git_filesystem/ConfigParser.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream file(argv[1]);
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>{file},{}));
  Json output=Json::array();for(const auto& item:cases){
    std::optional<std::string> subsection;
    if(item.contains("subsection")&&!item["subsection"].is_null())
      subsection=item["subsection"].get<std::string>();
    const auto result=git_filesystem::parseConfigString(item.at("config"),
      item.at("section"),subsection,item.at("key"));
    output.push_back(result?Json(*result):Json());
  }
  std::cout<<value::stringify(output)<<'\n';
}
