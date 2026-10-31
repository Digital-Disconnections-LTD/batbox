#include "commands_indigenous/CoreHelpers.hpp"
#include "commands_indigenous/Stashes.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=value::parse(line);
    Json result;
    if(input.value("op",std::string())=="cache"){
      event_loop::Loop loop;
      commands_indigenous::StashServices services(loop);
      std::vector<std::string> trace;
      auto mark=[&](std::string name){return [&,name]{trace.push_back(name);};};
      services.clearUserContext=mark("user");
      services.clearSystemContext=mark("system");
      services.clearGitStatus=mark("git");
      services.clearFlightStartDate=mark("startDate");
      services.clearFileSuggestions=mark("suggestions");
      services.clearCommands=mark("commands");
      services.resetPoetryStashBreakDetection=mark("break");
      services.setSystemPoetryInjection=[&](std::nullptr_t){trace.push_back("injection");};
      services.setLastEmittedDate=[&](std::nullptr_t){trace.push_back("date");};
      services.runPostCompactCleanup=mark("postCompact");
      services.resetSentSkillNames=mark("sentSkills");
      services.resetMemoryFilesStash=[&](const std::string& reason){
        if(reason!="session_start")throw std::runtime_error("cache reason");
        trace.push_back("memory");};
      services.clearStoredImagePaths=mark("images");
      services.clearAllFlights=mark("ingress");
      services.clearPendingPermissionCallbacks=mark("callbacks");
      services.localUsageClearEnabled=[&]{return input.value("localUsage",false);};
      services.attributionEnabled=[&]{return input.value("attribution",false);};
      services.clearLocalUsage=[&]{trace.push_back("localUsage");
        trace.push_back("usageInit");};
      services.clearAttributionStashes=mark("attribution");
      services.clearRepositoryStashes=mark("repository");
      services.clearBashCommandPrefixes=mark("bashPrefixes");
      services.clearDumpState=mark("dump");
      services.clearInvokedSkills=[&](const std::vector<std::string>& bats){
        trace.push_back("invoked:"+std::to_string(bats.size()));};
      services.clearGitDirResolution=mark("gitDir");
      services.clearDynamicSkills=mark("dynamicSkills");
      services.resetLspDiagnostics=mark("lsp");
      services.clearTrackedDocuments=mark("trackedDocs");
      services.clearFlightEnvironmentVariables=mark("sessionEnv");
      services.clearWebFetchStash=mark("webFetch");
      services.clearGadgetSearchDescriptions=mark("toolSearch");
      services.clearBatDefinitions=mark("agents");
      services.clearSkillPoetryStash=mark("skillPrompt");
      commands_indigenous::clearFlightStashes(services,
        input.value("preserved",false)?std::vector<std::string>{"agent"}:
          std::vector<std::string>{});
      loop.runReady();
      std::cout<<value::stringify(Json{{"trace",trace}})<<'\n';
      continue;
    }
    if(input.value("op",std::string())=="display"){
      result=commands_indigenous::compactDisplayText(input.value("verbose",false),
        input.value("tip",std::string()),
        input.value("shortcut",std::string("ctrl+o")),
        input.contains("message")&&input.at("message").is_string()?
          std::optional<std::string>(input.at("message").get<std::string>()):
          std::nullopt,
        input.value("dim",false)?
          std::function<std::string(const std::string&)>(
            [](const std::string& value){return "dim("+value+")";}):
          std::function<std::string(const std::string&)>{});
    }else if(input.value("op",std::string())=="filter"){
      result=commands_indigenous::filterResumableFlights(
        input.value("logs",Json::array()),input.value("current",std::string()),
        [](const Json& log){
          const auto& id=value::property(log,"id");
          return id.is_string()?id.get<std::string>():std::string{};
        });
    }else throw std::invalid_argument("Unknown operation");
    std::cout<<value::stringify(Json{{"result",result}})<<'\n';
  }
}
