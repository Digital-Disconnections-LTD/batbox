#include "skills/SkillGadgetSchema.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto result=input.at("op")=="schema"
      ?batbox::Json{{"input",batbox::skills::skillGadgetInputSchema()},
        {"output",batbox::skills::skillGadgetOutputSchema()}}
      :batbox::skills::parseSkillGadgetInput(input.at("value"));
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
