#include "commands/Roster.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto x=batbox::value::parse(line);
    std::vector<batbox::Json> commands;
    for(auto command:x.value("commands",batbox::Json::array())){
      if(command.contains("display"))command["userFacingName"]=command.at("display");
      commands.push_back(std::move(command));
    }
    batbox::Json parsed=nullptr;
    if(auto value=batbox::commands::parseSlashCommand(x.at("input").get<std::string>()))
      parsed={{"commandName",value->commandName},{"args",value->args},
        {"isMcp",value->isMcp}};
    const auto name=x.at("name").get<std::string>();
    const auto* found=batbox::commands::findCommand(name,commands);
    batbox::Json result={{"parsed",parsed},
      {"found",found?batbox::value::property(*found,"name"):batbox::Json()},
      {"has",batbox::commands::hasCommand(name,commands)}};
    try{result["got"]=batbox::commands::getCommand(name,commands).at("name");}
    catch(const std::exception& e){result["error"]=e.what();}
    std::cout<<batbox::value::stringify(result)<<'\n';
  }
}
