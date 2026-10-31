#include "application/ModelBinding.hpp"
#include "application_skills/Provider.hpp"
#include "application_flights/Flight.hpp"
#include "bats/ForkedBat.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "runtime/Platform.hpp"
#include "gadgets/live_roster/LiveRoster.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
void check(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
std::future<Json> ready(Json value){
  std::promise<Json> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
std::future<batbox::gadgets::GadgetResult> readyGadget(Json data){
  std::promise<batbox::gadgets::GadgetResult> promise;
  batbox::gadgets::GadgetResult result;result.data=std::move(data);
  promise.set_value(std::move(result));return promise.get_future();
}
std::string poetryText(const Json& blocks){
  check(blocks.is_array()&&!blocks.empty(),"Skill prompt did not return blocks");
  return blocks[0].at("text").get<std::string>();
}
}
int main(){
  using namespace batbox;
  const auto temp=std::filesystem::temp_directory_path()/
    ("batbox-skill-shell-"+platform::randomUUID());
  const auto skill=temp/".batbox"/"skills"/"fixture"/"SKILL.md";
  std::filesystem::create_directories(skill.parent_path());
  {std::ofstream file(skill);
    file<<"---\nname: fixture\ndescription: Shell expansion fixture\n"
      "allowed-tools: Bash\n---\nResult: !`printf native`\n";}
  try{
    event_loop::Loop owner;
    application::Config config;
    config.cwd=temp;config.stateDirectory=temp/"state";
    config.configDirectory=temp/"config";
    config.configPath=config.configDirectory/"config.json";
    config.model="offline-fixture";
    auto messages=std::make_shared<messages::Services>();
    messages->randomUUID=platform::randomUUID;
    messages->nowISO=[]{return platform::isoTimestamp();};
    messages->strictGadgetResultPairing=[]{return true;};
    messages->featureGate=[](const std::string&){return false;};
    messages->logError=[](const std::string&){};
    application_flights::Inputs flightInputs(owner);
    flightInputs.config=config;flightInputs.flightId="fixture-session";
    flightInputs.logError=[](std::exception_ptr){};
    flightInputs.commandNames=[]{return std::set<std::string>{};};
    auto flight=application_flights::Flight::create(std::move(flightInputs));
    auto model=application::makeLocalModel(owner,config,messages,
      [](const std::string&){});
    auto snapshot=std::make_shared<gadgets::live_roster::Snapshot>();
    std::atomic<int> permissionChecks=0,calls=0;
    std::atomic<bool> deny=false;
    gadgets::Gadget bash;bash.name="Bash";bash.maxResultSizeChars=30000;
    bash.checkPermissions=[&](const Json& input,const Json& state){
      ++permissionChecks;
      check(input.at("command")=="printf native","Skill command changed");
      const auto& allowed=state.at("toolPermissionContext")
        .at("alwaysAllowRules").at("command");
      check(allowed.is_array()&&allowed.size()==1&&allowed[0]=="Bash",
        "Skill allowed-tools did not reach Bash permission state");
      return ready(deny?Json{{"behavior","deny"},{"message","fixture denied"}}
                       :Json{{"behavior","allow"}});
    };
    bash.call=[&](const Json&,const gadgets::GadgetCallContext& context,
        const std::function<std::future<Json>(const Json&)>&,const Json&,
        const std::function<void(const gadgets::GadgetProgress&)>&){
      ++calls;
      check(!context.stopToken.stop_requested(),"Stopped skill ran Bash");
      return readyGadget({{"stdout","native-output"},{"stderr",""}});
    };
    bash.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,
        const std::string& id){
      return Json{{"type","tool_result"},{"tool_use_id",id},
        {"content",data.at("stdout")}};
    };
    snapshot->owned.push_back(std::move(bash));
    snapshot->base.push_back(&snapshot->owned.back());
    application_skills::Inputs inputs(owner);
    inputs.config=config;inputs.projectRoot=temp;
    inputs.messages=messages;inputs.flight=flight;inputs.model=model;
    inputs.invoked=std::make_shared<bootstrap::InvokedSkillsState>();
    inputs.currentMessages=[]{return Json::array();};
    inputs.mcpCommands=[]{return Json::array();};
    inputs.appState=[]{return Json{{"toolPermissionContext",Json::object()}};};
    inputs.currentBat=[]()->std::shared_ptr<bats::BatContext>{return {};};
    inputs.currentGadgetSnapshot=[snapshot]{return snapshot;};
    inputs.diagnostic=[](const std::string&){};
    inputs.resultStorage.getOriginalCwd=[temp]{return temp.string();};
    inputs.resultStorage.getFlightId=[flight]{return flight->id();};
    inputs.resultStorage.getProjectDir=[temp](const std::string&){return temp.string();};
    auto provider=application_skills::Provider::create(std::move(inputs));
    Json descriptor;
    for(const auto& row:provider->allCommands())
      if(row.at("name")=="fixture")descriptor=row;
    check(descriptor.is_object(),"Fixture skill was not discovered");
    const auto execute=[&](const bats::BatContext& context){
      auto hold=std::make_shared<event_loop::Loop::KeepAlive>(owner.hold());
      auto future=std::async(std::launch::async,[&,hold]{
        try{
          const auto result=provider->poetryForCommand(descriptor,"",context);
          owner.postMission([hold]{hold->release();});
          return result;
        }catch(...){owner.postMission([hold]{hold->release();});throw;}
      });
      owner.run();
      return future.get();
    };
    bats::BatContext initial;
    const auto first=poetryText(execute(initial));
    check(first.find("Result: native-output")!=std::string::npos,
      "First skill invocation did not expand through Bash result mapping");
    check(permissionChecks==1&&calls==1,"First invocation skipped Bash boundary");
    deny=true;
    bool denied=false;
    try{(void)execute(initial);}catch(const std::exception& error){
      denied=std::string(error.what()).find("fixture denied")!=std::string::npos;}
    check(denied&&permissionChecks==2&&calls==1,
      "Denied skill shell command reached Bash");
    deny=false;
    bats::BatContext active;
    active.query.gadgets=snapshot->base;
    active.query.gadgetSnapshotOwner=snapshot;
    const auto second=poetryText(execute(active));
    check(second.find("Result: native-output")!=std::string::npos&&
      permissionChecks==3&&calls==2,
      "Active AgentContext tool selection did not expand skill");
  }catch(...){std::filesystem::remove_all(temp);throw;}
  std::filesystem::remove_all(temp);
  std::cout<<"native skill shell provider workflow passed\n";
}
