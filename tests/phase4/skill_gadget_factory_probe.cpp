#include "skills/SkillGadgetFactory.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  batbox::skills::SkillGadgetFactoryServices services;
  services.getProjectRoot=[](){return std::string("/project");};
  services.getAllCommandsForValidation=[](const batbox::Json&){
    return std::vector<batbox::Json>{};};
  services.getRules=[](const batbox::Json&,const std::string&){
    return batbox::skills::RuleEntries{};};
  services.execution.getAllCommands=[](const batbox::bats::BatContext&){
    return std::vector<batbox::Json>{};};
  services.execution.lookup.findCommand=[](const std::string&,
    const std::vector<batbox::Json>&){return batbox::Json(nullptr);};
  services.renderGadgetUseMessage=[](const batbox::Json&,const batbox::Json&){
    return batbox::Json(nullptr);};
  services.renderGadgetResultMessage=[](const batbox::Json&,const batbox::Json&,
    const batbox::Json&){return batbox::Json(nullptr);};
  services.renderGadgetUseProgressMessage=[](const batbox::Json&,const batbox::Json&){
    return batbox::Json(nullptr);};
  services.renderGadgetUseRejectedMessage=services.renderGadgetUseProgressMessage;
  services.renderGadgetUseErrorMessage=services.renderGadgetUseProgressMessage;
  const auto gadget=batbox::skills::makeSkillGadget(std::move(services));
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto op=input.at("op").get<std::string>();
    batbox::Json result;
    if(op=="fields")result={{"name",gadget.name},{"searchHint",*gadget.searchHint},
      {"maxResultSizeChars",gadget.maxResultSizeChars},
      {"inputSchema",gadget.inputJSONSchema},{"outputSchema",gadget.outputSchema}};
    else if(op=="description")result=gadget.description(input.at("input"),{}).get();
    else if(op=="prompt")result=gadget.poetry({}).get();
    else if(op=="auto")result=gadget.toAutoClassifierInput(input.at("input"));
    else result=gadget.mapGadgetResultToGadgetResultBlockParam(input.at("data"),
      input.at("toolUseID"));
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
