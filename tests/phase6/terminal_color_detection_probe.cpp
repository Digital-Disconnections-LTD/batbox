#include "terminal_color/ColorDetection.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=3||std::string(argv[1])!="--cases")return 2;
  std::ifstream file(argv[2]);Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    terminal_color::ColorDetectionInputs in;
    const auto env=item.at("env");
    in.getenv=[env](const std::string& key)->std::optional<std::string>{
      const auto found=env.find(key);
      return found==env.end()?std::optional<std::string>{}:
        std::optional<std::string>{found->get<std::string>()};
    };
    in.argv={"batbox","fixture"};
    if(item.contains("argv"))for(const auto& value:item.at("argv"))
      in.argv.push_back(value.get<std::string>());
    in.platform="linux";in.streamIsTTY=item.at("tty").get<bool>();
    in.sniffFlags=item.value("sniffFlags",true);
    rows.push_back(Json{{"kind",item.at("kind")},
      {"level",terminal_color::detectColorLevel(in)}});
  }
  std::cout<<rows.dump()<<'\n';
}
