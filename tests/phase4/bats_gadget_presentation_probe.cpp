#include "bats/BatGadgetPresentation.hpp"
#include <iostream>
#include <string>
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const auto item=batbox::value::parse(line);
      batbox::Json out={{"result",batbox::bats::mapBatGadgetResult(item.at("data"),
        item.at("id").get<std::string>(),"Read","Bash")},
        {"classifier",batbox::bats::batGadgetAutoClassifierInput(item.at("input"))},
        {"activity",batbox::bats::batGadgetActivityDescription(item.at("input"))}};
      std::cout<<batbox::value::stringify(out)<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify(batbox::Json{{"error",error.what()}})<<'\n';
    }
  }
}
