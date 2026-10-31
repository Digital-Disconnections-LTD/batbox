#include "skills/SkillGadgetPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto op=input.at("op").get<std::string>();
    batbox::skills::SkillPoetryServices services;
    services.stringWidth=[](const std::string& s){return
      static_cast<double>(batbox::value::stringLength(s));};
    services.truncate=[](const std::string& s,double n){
      if(batbox::value::stringLength(s)<=n)return s;
      return batbox::value::slice(s,0,n-1)+"…";
    };
    services.getSkillGadgetCommands=[input](const std::string&){
      return input.value("commands",std::vector<batbox::Json>{});};
    services.getSlashCommandGadgetSkills=[input](const std::string&){
      if(input.value("error",false))throw std::runtime_error("discovery failed");
      return input.value("commands",std::vector<batbox::Json>{});};
    if(input.contains("envBudget")&&!input.at("envBudget").is_null())
      services.charBudgetEnvironment=input.at("envBudget").get<std::string>();
    std::optional<double> window;
    if(input.contains("window")&&!input.at("window").is_null())
      window=input.at("window").get<double>();
    batbox::Json result;
    if(op=="budget")result=batbox::skills::getSkillCharBudget(window,
      services.charBudgetEnvironment);
    else if(op=="description")result=batbox::skills::getCommandDescription(
      input.at("command"));
    else if(op=="format")result=batbox::skills::formatCommandDescription(
      input.at("command"));
    else if(op=="listing")result=batbox::skills::formatCommandsWithinBudget(
      input.value("commands",std::vector<batbox::Json>{}),window,services);
    else if(op=="prompt")result=batbox::skills::getSkillGadgetPoetry(
      input.value("cwd",std::string("/project")));
    else if(op=="limited")result=batbox::skills::getLimitedSkillGadgetCommands(
      "/project",services);
    else if(op=="info"){
      const auto value=batbox::skills::getSkillGadgetInfo("/project",services);
      result={{"totalCommands",value.total},{"includedCommands",value.included}};
    }else{
      const auto value=batbox::skills::getSkillInfo("/project",services);
      result={{"totalSkills",value.total},{"includedSkills",value.included}};
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
