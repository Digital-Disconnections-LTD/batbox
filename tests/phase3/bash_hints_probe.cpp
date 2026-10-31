#include "gadgets/bash/Hints.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string data(std::istreambuf_iterator<char>{file},{});
  const auto cases=batbox::value::parse(data);batbox::Json output=batbox::Json::array();
  for(const auto& item:cases){
    batbox::Json logs=batbox::Json::array();
    batbox::bash_gadget::HintProtocol protocol{"batbox-hint",[&](const std::string& message){logs.push_back(message);}};
    auto result=batbox::bash_gadget::extractBatboxHints(item.at("output").get<std::string>(),
      item.at("command").get<std::string>(),protocol);
    output.push_back({{"result",result},{"logs",logs}});
  }
  std::cout<<batbox::value::stringify(output)<<'\n';
}
