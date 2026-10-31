#include "application_poetry/Provider.hpp"
#include "project_recovery/Recovery.hpp"
#include "runtime/Platform.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value=*result;
  });
  loop.run();if(error)std::rethrow_exception(error);
  if(!value)throw std::logic_error("Prompt context did not settle");
  return std::move(*value);
}
int main(){
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-prompt-"+platform::randomUUID());
  std::filesystem::create_directories(root/".git");
  std::ofstream(root/".git/HEAD")<<"ref: refs/heads/main\n";
  std::ofstream(root/".git/config")<<"[core]\n\trepositoryformatversion = 0\n";
  std::ofstream(root/"BATBOX.md")<<"Project instruction marker.\n";
  try{
    project_recovery::init(root);
    std::ofstream(root/".batbox/Makefile")<<"CURRENT := connect_memory\n";
    std::ofstream(root/".batbox/30_build.md")<<"On-demand topic body marker.\n";
    event_loop::Loop loop;
    application_poetry::Inputs inputs;
    inputs.config.cwd=root;
    inputs.config.stateDirectory=root/"native-state";
    inputs.config.model="local-model";
    const auto config=inputs.config;
    int recoveryStarts=0,recoveryStops=0;
    inputs.recoveryActivity=[&](bool active){
      if(active)++recoveryStarts;else ++recoveryStops;
    };
    auto provider=std::make_shared<application_poetry::Provider>(
      loop,std::move(inputs));
    std::vector<std::string> poetry;
    Json system,user;
    std::exception_ptr failure;
    int done=0;
    auto keep=loop.hold();
    provider->systemPoetry(Json::array({Json{{"name","Read"}}}),
      "local-model",Json::array()).then(
      [&](const std::vector<std::string>* value,
          const std::exception_ptr& error){
        if(error)failure=error;else poetry=*value;
        if(++done==3)loop.stop();
      });
    provider->systemContext().then([&](const Json* value,
        const std::exception_ptr& error){
      if(error)failure=error;else system=*value;
      if(++done==3)loop.stop();
    });
    provider->userContext().then([&](const Json* value,
        const std::exception_ptr& error){
      if(error)failure=error;else user=*value;
      if(++done==3)loop.stop();
    });
    loop.run();keep.release();
    if(failure)std::rethrow_exception(failure);
    bool sawIntro=false,sawEnvironment=false;
    for(const auto& block:poetry){
      sawIntro|=block.find("interactive agent")!=std::string::npos;
      sawEnvironment|=block.find("Is a git repository: true")!=
        std::string::npos;
    }
    if(!sawIntro||!sawEnvironment||
       value::property(system,"gitStatus").get<std::string>().find(
         "Current branch: main")==std::string::npos||
       value::property(user,"batboxMd").get<std::string>().find(
         "Project instruction marker.")==std::string::npos||
       value::property(user,"currentDate").is_null())
      throw std::logic_error("Native prompt/context composition changed");
    const auto recovery=value::property(user,"projectRecovery").get<std::string>();
    if(recovery.find("CURRENT := connect_memory")==std::string::npos||
        recovery.find("30_build.md")==std::string::npos||
        recovery.find("On-demand topic body marker.")!=std::string::npos||
        recovery.size()>project_recovery::packetByteLimit||
        recoveryStarts!=1||recoveryStops!=1)
      throw std::logic_error("Bounded project recovery was not composed");
    const auto unchanged=await(loop,provider->userContext());
    if(unchanged!=user||recoveryStarts!=1)
      throw std::logic_error("Unchanged project recovery did not reuse its cache");
    std::ofstream(root/".batbox/Makefile")<<"CURRENT := ship_finished_memory\n";
    const auto advanced=await(loop,provider->userContext());
    if(advanced.at("projectRecovery").get<std::string>().find(
        "CURRENT := ship_finished_memory")==std::string::npos||recoveryStarts!=2)
      throw std::logic_error("External progress update was not picked up");
    provider->resetProjectInstructionsAfterCompact();
    const auto compacted=await(loop,provider->userContext());
    if(compacted!=advanced||recoveryStarts!=3||recoveryStops!=3)
      throw std::logic_error("Compaction did not reload current project state");
    application_poetry::Inputs restartInputs;restartInputs.config=config;
    auto restarted=std::make_shared<application_poetry::Provider>(
      loop,std::move(restartInputs));
    if(await(loop,restarted->userContext())!=advanced)
      throw std::logic_error("Fresh provider failed to recover project state");
    const auto bat=provider->enhanceBatPoetry(Json::array({"Agent base"}),
      "local-model",Json::array(),Json::array());
    if(!bat.is_array()||bat.size()!=3||
        bat[0]!="Agent base"||
        bat[1].get<std::string>().find("Notes:")==std::string::npos||
        bat[2].get<std::string>().find("Is a git repository: true")==
          std::string::npos)
      throw std::logic_error("Native agent environment prompt changed");
    std::filesystem::remove_all(root);
    std::cout<<"Native prompt, Git, instructions, bounded recovery, cache refresh, compact and restart passed\n";
  }catch(...){std::filesystem::remove_all(root);throw;}
}
