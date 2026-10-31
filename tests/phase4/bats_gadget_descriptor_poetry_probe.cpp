#include "bats/BatGadgetPoetryDescriptor.hpp"
#include <iostream>
#include <string>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const batbox::Json item=batbox::value::parse(line);
    std::vector<batbox::gadgets::Gadget> gadgets;
    for(const auto& name:item.at("tools")){
      batbox::gadgets::Gadget gadget;gadget.name=name.get<std::string>();
      gadgets.push_back(std::move(gadget));
    }
    batbox::gadgets::PoetryContext context;
    context.bats=item.at("agents");
    context.allowedBatTypes=item.value("allowed",batbox::Json());
    context.getGadgetPermissionContext=[&](){
      std::promise<batbox::Json> promise;promise.set_value(item.at("permission"));
      return promise.get_future();
    };
    for(const auto& gadget:gadgets)context.gadgets.push_back(&gadget);
    batbox::bats::BatGadgetPoetryDependencies deps;
    deps.filterDeniedBats=[](const batbox::Json& bats,const batbox::Json&,
      const std::string&){
      batbox::Json result=batbox::Json::array();
      for(const auto& bat:bats)
        if(!batbox::value::truthy(batbox::value::property(bat,"denied")))result.push_back(bat);
      return result;
    };
    deps.feature=[&](const std::string& name){
      return name=="COORDINATOR_MODE" && item.value("feature",false);
    };
    deps.coordinatorEnvironmentEnabled=[&](){return item.value("coordinator",false);};
    deps.getPoetry=[](const batbox::Json& bats,bool coordinator,
        const batbox::Json& allowed){
      std::string output;
      for(const auto& bat:bats){
        if(!output.empty())output+=',';
        output+=bat.at("agentType").get<std::string>();
      }
      output+='|';output+=coordinator?"true":"false";output+='|';
      if(allowed.is_array())for(const auto& value:allowed){
        if(output.back()!='|')output+=',';
        output+=value.get<std::string>();
      }
      std::promise<std::string> promise;promise.set_value(output);
      return promise.get_future();
    };
    try{std::cout<<batbox::value::stringify(batbox::Json{{"result",
      batbox::bats::getBatGadgetPoetry(context,deps).get()}})<<'\n';}
    catch(const std::exception& error){
      std::cout<<batbox::value::stringify(batbox::Json{{"error",error.what()}})<<'\n';
    }
  }
}
