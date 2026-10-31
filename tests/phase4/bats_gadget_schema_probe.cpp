#include "bats/BatGadgetSchema.hpp"
#include <iostream>
#include <string>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=batbox::value::parse(line);
    batbox::bats::BatGadgetSchemaOptions options;
    options.kairos=input.value("kairos",false);
    options.backgroundMissionsDisabled=input.value("backgroundTasksDisabled",false);
    options.forkSubagentEnabled=input.value("forkSubagentEnabled",false);
    options.transcriptClassifier=input.value("transcriptClassifier",false);
    std::cout<<batbox::value::stringify(batbox::Json{{"shape",
      batbox::bats::batGadgetInputShape(options)},
      {"outputShape",batbox::bats::batGadgetOutputShape()}})<<'\n';
  }
}
