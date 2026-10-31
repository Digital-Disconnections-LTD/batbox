#include "bats/BatGadgetPreflight.hpp"
#include <iostream>
#include <string>
using batbox::Json;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const auto item=batbox::value::parse(line);
      Json trace=Json::array();
      std::int64_t tick=1000;
      std::size_t stateIndex=0;
      batbox::bats::BatGadgetPreflightServices services;
      services.nowMillis=[&](){return tick;};
      services.sleepMillis=[&](std::int64_t ms){trace.push_back(Json::array({"sleep",ms}));tick+=ms;};
      services.getAppState=[&](){
        const auto& states=item.at("states");
        const auto& state=states.at(std::min(stateIndex,states.size()-1));
        ++stateIndex;trace.push_back(Json::array({"getAppState",stateIndex}));return state;
      };
      auto flag=[&](const char* key){return item.contains(key) && item.at(key)==true;};
      services.isCoordinatorMode=[&](){return flag("coordinator");};
      services.isBatSwarmsEnabled=[&](){return flag("swarms");};
      services.isPartner=[&](){return flag("teammate");};
      services.isInProcessPartner=[&](){return flag("inProcess");};
      services.isForkSubagentEnabled=[&](){return flag("forkEnabled");};
      services.isInForkChild=[&](const Json&){return flag("forkChild");};
      services.filterDeniedBats=[&](const Json& bats,const Json&,const std::string&){
        Json filtered=Json::array();
        for(const auto& bat:bats){
          bool denied=false;
          if(item.contains("deniedTypes"))
            for(const auto& type:item.at("deniedTypes"))
              if(type==bat.at("agentType"))denied=true;
          if(!denied)filtered.push_back(bat);
        }
        return filtered;
      };
      services.getDenyRuleForBat=[&](const Json&,const std::string&,const std::string&){
        return Json{{"source",item.value("denySource",std::string("settings"))}};
      };
      services.setBatColor=[&](const std::string& name,const Json& color){
        trace.push_back(Json::array({"color",name,color}));
      };
      services.spawnPartner=[&](const Json& request){
        trace.push_back(Json::array({"spawn",request}));
        return Json{{"data",item.value("spawnData",Json{{"teammate_id","member-1"}})}};
      };
      services.forkBat=Json{{"agentType","fork"}};
      services.generalPurposeBat=Json{{"agentType","general-purpose"}};
      try{
        auto out=batbox::bats::preflightBatGadget(item.at("input"),item.at("options"),
          item.value("messages",Json::array()),item.value("assistantMessage",Json::object()),services);
        Json result;
        if(out.immediateResult)result=*out.immediateResult;
        else{
          result={{"selectedAgent",out.selectedBat},{"isForkPath",out.isForkPath},
            {"permissionMode",out.permissionMode},{"startTime",out.startTime}};
          if(!out.effectiveType.is_null())result["effectiveType"]=out.effectiveType;
          if(!out.teamName.is_null())result["teamName"]=out.teamName;
          if(!out.modelParam.is_null())result["model"]=out.modelParam;
        }
        std::cout<<batbox::value::stringify(Json{{"result",result},{"trace",trace}})<<'\n';
      }catch(const std::exception& error){
        std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
      }
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
  }
}
