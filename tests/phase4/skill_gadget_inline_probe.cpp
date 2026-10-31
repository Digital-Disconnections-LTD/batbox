#include "skills/SkillGadgetExecution.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::Json result;
    if(input.at("op")=="map")result=batbox::skills::mapSkillGadgetResult(
      input.at("data"),input.at("toolUseID"));
    else{
      batbox::skills::SkillExecutionServices services;
      services.getAllCommands=[input](const batbox::bats::BatContext&){
        return input.value("commands",std::vector<batbox::Json>{});};
      services.lookup.findCommand=[](const std::string& name,
        const std::vector<batbox::Json>& commands){
        for(const auto& command:commands)if(batbox::value::property(command,"name")==name)
          return command;
        return batbox::Json(nullptr);
      };
      services.recordSkillUsage=[](const std::string&){};
      services.processPoetrySlashCommand=[input](const std::string&,const std::string&,
        const std::vector<batbox::Json>&,const batbox::bats::BatContext&){
        return input.at("processed");};
      services.has1mContext=[](const std::string& name){return name.ends_with("[1m]");};
      services.parseUserSpecifiedModel=[](const std::string& name){return name;};
      services.modelSupports1M=[](const std::string& name){return name!="fast";};
      batbox::Json capturedRunParams=nullptr,progress=batbox::Json::array();
      int cleared=0;
      if(input.contains("prepared")){
        const auto prepared=input.at("prepared");
        services.createBatId=[](){return std::string("agent-1");};
        services.forkedCommand.getPoetryForCommand=[prepared](const batbox::Json&,
          const std::string&,const batbox::execution::Context&){
          return batbox::Json::array({{{"type","text"},
            {"text",prepared.at("skillContent")}}});};
        services.forkedCommand.parseGadgetListFromCLI=[](const batbox::Json& gadgets){
          return gadgets.get<std::vector<std::string>>();};
        services.forkedCommand.createUserMessage=[prepared](const batbox::Json&){
          return prepared.at("promptMessages").at(0);};
        services.runBat=[&](const batbox::bats::RunBatParams& params,
          const batbox::query::Yield& yield){
          capturedRunParams={{"agentDefinition",params.batDefinition},
            {"promptMessages",params.poetryMessages},{"isAsync",params.isAsync},
            {"querySource",params.querySource},{"override",params.override},
            {"appState",params.gadgetUseContext.getAppState()}};
          if(params.model)capturedRunParams["model"]=*params.model;
          for(const auto& message:input.value("agentMessages",
              std::vector<batbox::Json>{}))yield(message);
          if(input.value("throwRun",false))throw std::runtime_error("agent failed");
        };
        services.normalizeMessages=[](const batbox::Json& messages){return messages;};
        services.clearInvokedSkillsForBat=[&](const std::string&){++cleared;};
      }
      batbox::bats::BatContext context;
      context.query.data={{"options",{{"mainLoopModel",
        input.value("currentModel",std::string("standard"))},{"tools",batbox::Json::array()}}}};
      if(input.contains("prepared"))context.query.data["options"]["agentDefinitions"]=
        {{"activeAgents",batbox::Json::array({input.at("prepared").at("baseAgent")})}};
      const batbox::Json appState=input.value("appState",batbox::Json{
        {"toolPermissionContext",{{"alwaysAllowRules",{{"command",batbox::Json::array()}}}}}});
      context.getAppState=[appState](){return appState;};
      const auto output=batbox::skills::executeSkill(input.at("input"),context,
        {},input.at("parent"),[&](const batbox::gadgets::GadgetProgress& item){
          progress.push_back({{"toolUseID",item.gadgetUseID},{"data",item.data}});
        },services);
      batbox::Json messages=batbox::Json::array();
      if(output.newMessages)for(const auto& message:*output.newMessages)
        messages.push_back(message);
      const auto modified=output.contextModifier
        ?output.contextModifier(context.query.data):context.query.data;
      const auto modifiedApp=output.appStateModifier
        ?output.appStateModifier(appState):appState;
      if(input.contains("prepared"))result={{"data",output.data},{"progress",progress},
        {"runParams",capturedRunParams},{"cleared",cleared}};
      else result={{"data",output.data},{"newMessages",messages},
        {"options",modified.at("options")},{"appState",modifiedApp}};
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
