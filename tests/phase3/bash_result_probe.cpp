#include "gadgets/bash/BashGadget.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  const auto value=batbox::bash_gadget::mapGadgetResult(input.at("output"),
    input.value("toolUseID",std::string("toolu_1")),
    [](const std::string& id){return "/tmp/batbox-jobs/"+id+".output";},15000);
  std::cout<<batbox::value::stringify(Json{{"result",value}})<<'\n';
}catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"fatal",e.what()}})<<'\n';}}
