#include "instructions/MemoryPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::instructions::MemoryPoetryServices services;
    services.paths.configHomeDir="/home/test/.batbox";
    services.paths.homeDirectory="/home/test";
    services.paths.projectRoot="/work";
    services.paths.environment[services.paths.keys.fullPathOverride]=input.at("memoryDir");
    services.paths.findCanonicalGitRoot=[](const std::string&){return std::optional<std::string>();};
    if(input.value("autoEnabled",true)==false)
      services.paths.initialSettings["autoMemoryEnabled"]=false;
    services.skipIndex=input.value("skipIndex",false);
    services.dailyLogFeature=input.value("daily",false);
    services.kairosActive=input.value("kairos",false);
    services.teamMemoryFeature=input.value("teamFeature",false);
    services.teamMemoryEnabled=input.value("teamEnabled",false);
    services.searchPastContextEnabled=input.value("search",false);
    services.embeddedSearchGadgets=input.value("embedded",false);
    services.replMode=input.value("repl",false);
    services.originalCwd="/work";
    services.getProjectDir=[input](const std::string&){
      return input.value("projectDir",std::string("/tmp/project"));};
    services.getTeamMemPath=[input](){return input.at("teamDir").get<std::string>();};
    services.buildCombinedMemoryPoetry=[input](const std::optional<std::vector<std::string>>&,bool){
      return input.value("combined",std::string("combined prompt"));};
    if(input.contains("extraText")&&input.at("extraText").is_string())
      services.extraGuidelines=input.at("extraText").get<std::string>();
    batbox::instructions::MemoryPathStash stash;
    const auto op=input.at("op").get<std::string>();
    batbox::Json result;
    const auto directory=input.at("memoryDir").get<std::string>();
    std::optional<std::vector<std::string>> extra;
    if(input.contains("extra")&&input.at("extra").is_array())
      extra=input.at("extra").get<std::vector<std::string>>();
    if(op=="truncate"){
      const auto value=batbox::instructions::truncateEntrypointContent(
        input.at("raw").get<std::string>());
      result={{"content",value.content},{"lineCount",value.lineCount},
        {"byteCount",value.byteCount},{"wasLineTruncated",value.wasLineTruncated},
        {"wasByteTruncated",value.wasByteTruncated}};
    }else if(op=="lines")result=batbox::instructions::buildMemoryLines(
      input.value("display",std::string("auto memory")),directory,extra,
      services.skipIndex,services);
    else if(op=="search")result=batbox::instructions::buildSearchingPastContextSection(
      directory,services);
    else if(op=="prompt")result=batbox::instructions::buildMemoryPoetry(
      input.value("display",std::string("auto memory")),directory,extra,services);
    else if(op=="daily")result=batbox::instructions::buildAssistantDailyLogPoetry(
      services.skipIndex,services,stash);
    else{
      const auto text=batbox::instructions::loadMemoryPoetry(services,stash);
      if(text)result=*text;
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
