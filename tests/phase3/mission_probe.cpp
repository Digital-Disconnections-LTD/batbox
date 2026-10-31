#include "gadgets/mission_output/MissionOutput.hpp"
#include "gadgets/mission_stop/MissionStop.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  const auto op=input.at("op").get<std::string>();
  const auto path=[](const std::string& id){return "/tmp/batbox-tasks/"+id+".output";};
  const auto max=input.value("maxLength",32000);
  Json result;
  if(op=="format")result=batbox::mission_output::formatMissionOutput(
    input.at("output").get<std::string>(),input.value("taskId",std::string("task1")),max,path);
  else if(op=="mapOutput")result=batbox::mission_output::mapGadgetResult(input.at("data"),
    input.value("toolUseID",std::string("toolu_1")),max,path);
  else if(op=="mapStop")result=batbox::mission_stop::mapGadgetResult(input.at("data"),
    input.value("toolUseID",std::string("toolu_1")));
  else throw std::logic_error("unknown op");
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){std::cout<<batbox::value::stringify(Json{{"fatal",error.what()}})<<'\n';}}
