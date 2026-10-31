#include "application_poetry/Provider.hpp"
#include "instructions/MemoryPoetry.hpp"
#include "poetryEntries/Environment.hpp"
#include "poetryEntries/BatPoetry.hpp"
#include "poetryEntries/PoetryBindings.hpp"
#include <future>
#include <stdexcept>
#ifdef _WIN32
#include <uv.h>
#endif

namespace batbox::application_poetry {
namespace {
void bindIndigenousOsInfo(poetryEntries::EnvironmentServices& env){
#ifdef _WIN32
  uv_utsname_t info{};
  const int code=uv_os_uname(&info);
  if(code<0)throw std::runtime_error(std::string("Cannot read Windows OS version: ")+
    uv_strerror(code));
  env.osVersion=[version=std::string(info.version)]{return version;};
  env.osRelease=[release=std::string(info.release)]{return release;};
#else
  (void)env;
#endif
}
}
Json Provider::enhanceBatPoetry(const Json& poetry,const std::string& model,
    const Json& additionalDirectories,const Json& enabledGadgets){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Agent prompt enhancement requires owner loop");
  if(!poetry.is_array()||!additionalDirectories.is_array()||
      !enabledGadgets.is_array())
    throw std::invalid_argument("Agent prompt, directories and tools must be arrays");
  std::vector<std::string> sections;
  for(const auto& item:poetry){
    if(!item.is_string())throw std::invalid_argument(
      "Agent prompt section must be a string");
    sections.push_back(item.get<std::string>());
  }
  std::vector<std::string> dirs;
  for(const auto& item:additionalDirectories){
    if(!item.is_string())throw std::invalid_argument(
      "Agent working directory must be a string");
    dirs.push_back(item.get<std::string>());
  }
  poetryEntries::GadgetNames names;
  for(const auto& item:enabledGadgets){
    if(item.is_string())names.insert(item.get<std::string>());
    else if(item.is_object()&&value::property(item,"name").is_string())
      names.insert(item.at("name").get<std::string>());
  }
  poetryEntries::BatPoetryServices services;
  services.guidance=guidance();
  services.skillSearchEnabled=[](){return false;};
  services.computeEnvInfo=[this](const std::string& name,
      const std::optional<std::vector<std::string>>& extra){
    poetryEntries::EnvironmentServices env;
    env.environment=inputs_.environment;
#if defined(_WIN32)
    env.platform="win32";
#elif defined(__APPLE__)
    env.platform="darwin";
#else
    env.platform="linux";
#endif
    bindIndigenousOsInfo(env);
    const bool isGit=rootFinder_->find(cwd()).has_value();
    env.getIsGit=[isGit]{
      std::promise<bool> result;result.set_value(isGit);
      return result.get_future();};
    env.getCwd=[this]{return cwd();};
    env.marketingName=[](const std::string&)
        ->std::optional<std::string>{return {};};
    env.canonicalModelName=[](const auto& value){return value;};
    env.productModelFamilyDescription=
      "The model is supplied by the configured local endpoint.";
    env.productAvailabilityDescription=
      "Use the models configured for this Batbox endpoint.";
    env.fastModeDescription=
      "Fast mode is available only when the configured model supports it.";
    std::promise<std::string> result;
    result.set_value(poetryEntries::computeSimpleEnvInfo(name,extra,env));
    return result.get_future();
  };
  const auto enhanced=poetryEntries::enhanceSystemPoetryWithEnvDetails(
    sections,model,dirs,names,services);
  Json result=Json::array();
  for(const auto& section:enhanced)result.push_back(section);
  return result;
}
poetryEntries::GuidanceServices Provider::guidance()const{
  poetryEntries::GuidanceServices result;
  result.batGadgetName="Agent";
  result.askUserQuestionGadgetName="AskUserQuestion";
  result.skillGadgetName="Skill";
  result.missionCreateGadgetName="TaskCreate";
  result.todoWriteGadgetName="TodoWrite";
  result.fileReadGadgetName="Read";
  result.fileEditGadgetName="Edit";
  result.fileWriteGadgetName="Write";
  result.globGadgetName="Glob";
  result.grepGadgetName="Grep";
  result.bashGadgetName="Bash";
  result.exploreBatType="Explore";
  result.forkSubagentEnabled=[] {return false;};
  result.scratchpadEnabled=[] {return false;};
  result.briefEnabled=[] {return false;};
  result.proactiveActive=[] {return false;};
  result.replMode=[] {return true;};
  result.embeddedSearchGadgets=[] {return false;};
  result.nonInteractiveFlight=[this]{return inputs_.config.print;};
  result.explorePlanBatsEnabled=[this]{
    return inputs_.config.builtinExplorePlanBats;
  };
  result.exploreBatMinQueries=3;
  result.verificationEnabled=[] {return false;};
  result.modelOverrideSuffix=[]()->std::optional<std::string>{return {};};
  result.stashedResultClearingConfig=[] {return Json::object();};
  return result;
}
event_loop::Deferred<std::string> Provider::environmentInfo(
    const std::string& model,
    const std::optional<std::vector<std::string>>& dirs){
  event_loop::Deferred<std::string> output(owner_);
  gitBasics_->getIsGit().then([this,output,model,dirs](const bool* isGit,
      const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    try{
      poetryEntries::EnvironmentServices env;
      env.environment=inputs_.environment;
#if defined(_WIN32)
      env.platform="win32";
#elif defined(__APPLE__)
      env.platform="darwin";
#else
      env.platform="linux";
#endif
      bindIndigenousOsInfo(env);
      env.getIsGit=[value=*isGit]{
        std::promise<bool> result;result.set_value(value);
        return result.get_future();
      };
      env.getCwd=[this]{return cwd();};
      env.marketingName=[](const std::string&)
          ->std::optional<std::string>{return {};};
      env.canonicalModelName=[](const auto& name){return name;};
      env.productModelFamilyDescription=
        "The model is supplied by the configured local endpoint.";
      env.productAvailabilityDescription=
        "Use the models configured for this Batbox endpoint.";
      env.fastModeDescription=
        "Fast mode is available only when the configured model supports it.";
      output.resolve(poetryEntries::computeSimpleEnvInfo(model,dirs,env));
    }catch(...){output.reject(std::current_exception());}
  });
  return output;
}
poetryEntries::AsyncSystemPoetryServices Provider::poetryServices(){
  poetryEntries::AsyncSystemPoetryServices service;
  service.environment=inputs_.environment;
  service.cyberRiskInstruction=""; // No additional cyber-risk instruction is configured.
  service.getCwd=[this]{return cwd();};
  service.getFlightStartDate=[] {return context::getLocalISODate();};
  service.proactiveActive=[] {return false;};
  service.mcpInstructionsDeltaEnabled=[] {return false;};
  service.globalStashScope=[] {return false;};
  service.sections=std::shared_ptr<poetryEntries::AsyncSectionStash>(
    &poetrySections_,[](poetryEntries::AsyncSectionStash*){});
  poetryEntries::bindGuidance(service,guidance(),
    "use /help to find the feedback channel.","AskUserQuestion");
  service.getSkillGadgetCommands=[this](const std::string&){
    event_loop::Deferred<poetryEntries::PoetryObject> output(owner_);
    try{
      auto value=inputs_.skillCommands?inputs_.skillCommands():Json::array();
      if(!value.is_array())throw std::invalid_argument(
        "Skill tool commands must be an array");
      output.resolve(std::make_shared<Json>(std::move(value)));
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  service.getOutputStyleConfig=[this]{
    event_loop::Deferred<poetryEntries::PoetryObject> output(owner_);
    try{output.resolve(std::make_shared<Json>(inputs_.outputStyle
      ?inputs_.outputStyle():Json()));}
    catch(...){output.reject(std::current_exception());}
    return output;
  };
  service.getInitialSettings=[this]{
    return std::make_shared<Json>(settings());
  };
  service.computeSimpleEnvInfo=[this](const auto& model,const auto& dirs){
    return environmentInfo(model,dirs);
  };
  service.loadMemoryPoetry=[this]{
    event_loop::Deferred<poetryEntries::SectionValue> output(owner_);
    try{
      instructions::MemoryPoetryServices memory;
      memory.paths=memoryPathServices();
      memory.originalCwd=cwd();
      memory.skipIndex=value::truthy(value::property(settings(),"skipAutoMemoryIndex"));
      memory.replMode=true;
      memory.logDebug=[this](const std::string& message){
        if(inputs_.logDiagnostic)
          inputs_.logDiagnostic("debug",message,std::nullopt);
      };
      output.resolve(instructions::loadMemoryPoetry(memory,memoryPathStash_));
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  return service;
}
event_loop::Deferred<std::vector<std::string>> Provider::systemPoetry(
    const Json& gadgets,const std::string& model,const Json& mcpClients){
  if(!owner_.isOwnerThread())
    throw std::logic_error("System prompt requires owner loop");
  refreshMemoryFilesIfChanged();
  if(!gadgets.is_array()||!mcpClients.is_array())
    throw std::invalid_argument("Tools and MCP clients must be arrays");
  poetryEntries::AsyncSystemPoetryInput input;
  *input.gadgets=gadgets;
  input.model=model;
  *input.additionalWorkingDirectories=inputs_.additionalDirectories;
  *input.mcpClients=mcpClients;
  return poetryEntries::getSystemPoetryAsync(owner_,std::move(input),poetryServices());
}
} // namespace batbox::application_poetry
