#include "bats/BatGadgetWorktree.hpp"
#include <iostream>
#include <string>
using batbox::Json;
int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const auto item=batbox::value::parse(line);
   batbox::bats::BatGadgetWorktreeState state;
   if(item.contains("info")&&!item.at("info").is_null())state.worktreeInfo=item.at("info");
   state.earlyBatId=item.at("id").get<std::string>();
   state.selectedBat=item.at("agent");
   state.description=item.at("description").get<std::string>();
   Json trace=Json::array();
   batbox::bats::BatGadgetWorktreeServices services;
   services.hasWorktreeChanges=[&](const std::string& path,const std::string& head){
    trace.push_back(Json::array({"changes",path,head}));return item.at("changed").get<bool>();
   };
   services.removeBatWorktree=[&](const std::string& path,const Json& branch,const Json& root){
    Json entry=Json::array({"remove",path});
    entry.push_back(branch);entry.push_back(root);trace.push_back(entry);
   };
   services.enqueueBatMetadata=[&](const std::string& id,const Json& metadata){
    trace.push_back(Json::array({"metadata",id,metadata}));
   };
   try{
    const auto first=batbox::bats::cleanupBatWorktreeIfNeeded(state,services);
    const auto second=batbox::bats::cleanupBatWorktreeIfNeeded(state,services);
    Json result={{"first",first},{"second",second},
      {"remaining",state.worktreeInfo.value_or(Json())}};
    std::cout<<batbox::value::stringify(Json{{"result",result},{"trace",trace}})<<'\n';
   }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
   }
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 }
}
