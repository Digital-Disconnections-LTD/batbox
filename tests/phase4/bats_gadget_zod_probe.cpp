#include "bats/BatGadgetSchema.hpp"
#include <iostream>
#include <string>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto item=batbox::value::parse(line);
    batbox::bats::BatGadgetSchemaOptions options;
    options.kairos=item.value("kairos",false);
    options.backgroundMissionsDisabled=item.value("backgroundTasksDisabled",false);
    options.forkSubagentEnabled=item.value("forkSubagentEnabled",false);
    options.transcriptClassifier=item.value("transcriptClassifier",false);
    batbox::Json output={{"inputSchema",batbox::bats::batGadgetInputJSONSchema(options)},
      {"outputSchema",batbox::bats::batGadgetOutputJSONSchema()}};
    if(item.contains("input")){
      const auto parsed=batbox::bats::inspectBatGadgetInput(item.at("input"),options);
      output["parsed"]=parsed.at("success")==true?
        parsed.at("data"):batbox::Json{{"error",parsed.at("error")}};
    }
    std::cout<<batbox::value::stringify(output)<<'\n';
  }
}
