#include "gadgets/mission_management/Gadgets.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <set>
#include <thread>

using batbox::Json;
using namespace batbox::mission_management;

namespace {
const batbox::gadgets::Gadget& byName(const std::vector<batbox::gadgets::Gadget>& gadgets,
    const std::string& name){
  for(const auto& gadget:gadgets)if(gadget.name==name)return gadget;
  throw std::runtime_error("missing tool "+name);
}
Json call(const batbox::gadgets::Gadget& gadget,const Json& input){
  const auto parsed=gadget.parseInput(input);
  return gadget.call(parsed,{}, {},Json::object(),{}).get().data;
}
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])=="--fixture-json"){
    const auto root=std::filesystem::temp_directory_path()/
      ("batbox-mission-fixture-"+std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try{
      auto store=std::make_shared<Store>(root);
      bool blockCreated=false,blockCompleted=false;
      Services services;services.store=store;
      services.missionListId=[] {return std::string("mission-fixture");};
      services.lifecycleHooks=[&](const std::string& event,const Json&,
          const batbox::gadgets::GadgetCallContext&){
        if(event=="TaskCreated"&&blockCreated)
          return std::vector<std::string>{"TaskCreated hook feedback:\nblocked"};
        if(event=="TaskCompleted"&&blockCompleted)
          return std::vector<std::string>{"TaskCompleted hook feedback:\nblocked"};
        return std::vector<std::string>{};
      };
      const auto gadgets=makeMissionGadgets(services);
      const auto actions=Json::parse(std::cin);Json output=Json::array();
      for(const auto& action:actions){
        const auto name=action.at("tool").get<std::string>();
        if(name=="hooks"){
          blockCreated=action.value("create",false);
          blockCompleted=action.value("complete",false);
          continue;
        }
        try{
          const auto& gadget=byName(gadgets,name);
          const auto data=call(gadget,action.at("input"));
          const auto block=gadget.mapGadgetResultToGadgetResultBlockParam(data,"use");
          output.push_back({{"data",data},{"content",block.at("content")}});
        }catch(const std::exception& error){output.push_back({{"error",error.what()}});}
      }
      std::cout<<output.dump()<<'\n';std::filesystem::remove_all(root);return 0;
    }catch(...){std::filesystem::remove_all(root);throw;}
  }
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-task-fixture-"+std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  try{
    auto store=std::make_shared<Store>(root);
    std::atomic<int> createdHooks{0},completedHooks{0},expansions{0};
    bool rejectCreate=true,rejectComplete=true;
    Services services;
    services.store=store;services.missionListId=[] {return std::string("session/one");};
    services.expandMissions=[&]{++expansions;};
    services.lifecycleHooks=[&](const std::string& event,const Json& detail,
        const batbox::gadgets::GadgetCallContext&){
      assert(detail.contains("task_id")&&detail.contains("task_subject")&&
        detail.contains("task_description"));
      if(event=="TaskCreated"){
        ++createdHooks;
        return rejectCreate?std::vector<std::string>{"TaskCreated hook feedback:\nblocked"}:
          std::vector<std::string>{};
      }
      assert(event=="TaskCompleted");++completedHooks;
      return rejectComplete?std::vector<std::string>{"TaskCompleted hook feedback:\nblocked"}:
        std::vector<std::string>{};
    };
    const auto gadgets=makeMissionGadgets(services);
    const auto& create=byName(gadgets,"TaskCreate");
    const auto& get=byName(gadgets,"TaskGet");
    const auto& update=byName(gadgets,"TaskUpdate");
    const auto& list=byName(gadgets,"TaskList");
    assert(create.shouldDefer&&*create.shouldDefer);
    assert(get.isReadOnly(Json::object())&&list.isReadOnly(Json::object()));
    // Result renderers receive (output, progress, options); the card shows the output.
    for(const auto* gadget:{&create,&get,&update,&list}){
      const auto shown=gadget->renderGadgetResultMessage(Json("Mission #1 created"),
        Json::array({Json("progress must not be shown")}),Json::object());
      if(shown!=Json("Mission #1 created"))
        throw std::runtime_error(gadget->name+" rendered "+shown.dump()+" instead of its output");
    }
    try{(void)call(create,{{"subject","A"},{"description","First"}});
      throw std::runtime_error("created hook did not block");}
    catch(const std::runtime_error& error){
      assert(std::string(error.what())=="TaskCreated hook feedback:\nblocked");}
    assert(store->list("session/one").empty());
    rejectCreate=false;
    const auto first=call(create,{{"subject","A"},{"description","First"},
      {"metadata",{{"priority",1}}}});
    const auto second=call(create,{{"subject","B"},{"description","Second"}});
    assert(first["task"]["id"]=="2"&&second["task"]["id"]=="3");
    assert(createdHooks==3&&expansions==2);
    assert(get.mapGadgetResultToGadgetResultBlockParam(
      call(get,{{"taskId","missing"}}),"use")["content"]=="Task not found");
    const auto block=call(update,{{"taskId","3"},{"addBlockedBy",Json::array({"2"})},
      {"owner","worker"},{"metadata",{{"priority",2},{"note","keep"}}}});
    assert(block["updatedFields"]==Json::array({"owner","metadata","blockedBy"}));
    assert(store->get("session/one","2")->at("blocks")==Json::array({"3"}));
    assert(store->get("session/one","3")->at("blockedBy")==Json::array({"2"}));
    const auto blockedList=call(list,Json::object());
    assert(blockedList["tasks"].size()==2);
    assert(blockedList["tasks"][1]["blockedBy"]==Json::array({"2"}));
    const auto denied=call(update,{{"taskId","2"},{"status","completed"}});
    assert(denied["success"]==false&&denied["error"]==
      "TaskCompleted hook feedback:\nblocked");
    assert(store->get("session/one","2")->at("status")=="pending");
    rejectComplete=false;
    const auto done=call(update,{{"taskId","2"},{"status","completed"}});
    assert(done["statusChange"]["from"]=="pending"&&
      done["statusChange"]["to"]=="completed");
    assert(completedHooks==2);
    assert(call(list,Json::object())["tasks"][1]["blockedBy"].empty());
    const auto merged=call(update,{{"taskId","2"},
      {"metadata",{{"priority",nullptr},{"note","final"}}}});
    assert(merged["updatedFields"]==Json::array({"metadata"}));
    const auto mission=store->get("session/one","2");
    assert(!mission->at("metadata").contains("priority")&&
      mission->at("metadata")["note"]=="final");
    const auto removed=call(update,{{"taskId","2"},{"status","deleted"}});
    assert(removed["success"]==true&&removed["statusChange"]["to"]=="deleted");
    assert(store->get("session/one","3")->at("blockedBy").empty());
    Store reopened(root);
    assert(reopened.list("session/one").size()==1);
    assert(reopened.create("session/one",{{"subject","C"},{"description","Third"},
      {"status","pending"},{"blocks",Json::array()},{"blockedBy",Json::array()}})=="4");
    std::vector<std::future<std::string>> concurrent;
    for(int i=0;i<12;++i)concurrent.push_back(std::async(std::launch::async,[&]{
      Store other(root);
      return other.create("session/one",{{"subject","parallel"},
        {"description","worker"},{"status","pending"},
        {"blocks",Json::array()},{"blockedBy",Json::array()}});
    }));
    std::set<std::string> ids;
    for(auto& result:concurrent)ids.insert(result.get());
    assert(ids.size()==12&&reopened.list("session/one").size()==14);
    bool rejected=false;
    try{(void)list.parseInput({{"unknown",1}});}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::filesystem::remove_all(root);
    std::cout<<"Mission management contract checks passed\n";
  }catch(...){std::filesystem::remove_all(root);throw;}
}
