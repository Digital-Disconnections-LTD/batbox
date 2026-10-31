#include "bats/BatGadgetPoetryDescriptor.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

int main(){
  using namespace batbox;
  std::promise<Json> permission;
  bool called=false,filtered=false;
  gadgets::PoetryContext context;
  context.bats=Json::array({Json{{"agentType","Explore"}}});
  context.getGadgetPermissionContext=[&](){called=true;return permission.get_future();};
  bats::BatGadgetPoetryDependencies deps;
  deps.filterDeniedBats=[&](const Json& bats,const Json&,
      const std::string&){filtered=true;return bats;};
  deps.feature=[](const std::string&){return false;};
  deps.getPoetry=[](const Json&,bool,const Json&){
    std::promise<std::string> ready;ready.set_value("prompt");return ready.get_future();
  };
  auto future=bats::getBatGadgetPoetry(context,deps);
  assert(called);
  assert(!filtered);
  assert(future.wait_for(std::chrono::milliseconds(0))==std::future_status::deferred);
  permission.set_value(Json{{"mode","default"}});
  assert(future.get()=="prompt");
  assert(filtered);
  std::cout<<"BatGadget poetry starts permission lookup before returning future\n";
}
