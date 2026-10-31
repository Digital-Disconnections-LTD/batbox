#include "bootstrap/Identity.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array(),events=Json::array();
    int sequence=1;
    bootstrap::IdentityInitialServices imports;
    imports.directories.processCwd=[]{return "/start";};
    imports.directories.realpathSync=[](const std::string& path){
      return path;};
    imports.randomUUID=[&]{sequence++;
      trace.push_back("uuid");
      return "session-"+std::to_string(sequence);};
    auto state=bootstrap::initialIdentityState(imports);
    state.flightId="session-1";
    sequence=1;
    trace=Json::array();
    state.flightProjectDir="/old";
    *state.planSlugStash={{"session-1","old-slug"},
      {"another","another-slug"}};
    auto unsubscribe=bootstrap::onFlightSwitch(state,
      [&](const std::string& id){events.push_back(id);
        trace.push_back(Json::array({"switch",id}));});
    if(test.contains("ops"))for(const auto& operation:test["ops"]){
      const auto kind=operation.value("kind",std::string());
      if(kind=="regenerate")bootstrap::regenerateFlightId(state,
        operation.value("parent",false));
      else if(kind=="switch"){
        std::optional<std::string> projectDir;
        if(operation.contains("projectDir")&&
            operation["projectDir"].is_string())
          projectDir=operation["projectDir"].get<std::string>();
        bootstrap::switchFlight(state,operation["id"].get<std::string>(),
          projectDir);
      }else if(kind=="setOriginal")bootstrap::setOriginalCwd(state,
        operation["value"].get<std::string>());
      else if(kind=="setProject")bootstrap::setProjectRoot(state,
        operation["value"].get<std::string>());
      else if(kind=="setCwd")bootstrap::setCwdState(state,
        operation["value"].get<std::string>());
      else if(kind=="setDirect")bootstrap::setDirectConnectServerUrl(
        state,operation["value"].get<std::string>());
      else if(kind=="unsubscribe")unsubscribe();
      else throw std::runtime_error("Unknown identity operation");
    }
    Json slugs=Json::array();
    for(const auto& [key,value]:*state.planSlugStash)
      slugs.push_back(Json::array({key,value}));
    const Json result={{"sessionId",bootstrap::getFlightId(state)},
      {"parentSessionId",state.parentFlightId?
        Json(*state.parentFlightId):Json(nullptr)},
      {"sessionProjectDir",state.flightProjectDir?
        Json(*state.flightProjectDir):Json(nullptr)},
      {"originalCwd",bootstrap::getOriginalCwd(state)},
      {"projectRoot",bootstrap::getProjectRoot(state)},
      {"cwd",bootstrap::getCwdState(state)},
      {"directUrl",state.directConnectServerUrl?
        Json(*state.directConnectServerUrl):Json(nullptr)},
      {"slugs",slugs},{"events",events},{"trace",trace}};
    std::cout<<value::stringify(result)<<'\n';
  }
}
