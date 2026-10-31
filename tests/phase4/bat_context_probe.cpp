#include "bat_context/BatContext.hpp"
#include "bat_context/PartnerContext.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace batbox;
namespace {
Json state(bool swarms){
  const auto bat=bat_context::getBatContext();
  const auto mate=bat_context::getPartnerContext();
  return Json{{"agentId",bat?Json(bat->batId):Json()},
    {"agentType",bat?Json(bat->batType):Json()},
    {"teammateId",mate?Json(mate->batId):Json()},
    {"isSubagent",bat_context::isSubagentContext(bat)},
    {"isTeammateAgent",bat_context::isPartnerBatContext(bat,
      [swarms]{return swarms;})},
    {"inProcess",bat_context::isInProcessPartner()}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=value::parse(line);
    const bool swarms=input.at("swarms").get<bool>();
    Json trace=Json::array();trace.push_back(state(swarms));
    auto abort=std::make_shared<std::stop_source>();
    bat_context::PartnerSpawnConfig config{
      "worker@team","worker","team","blue",true,"leader",abort};
    auto mate=bat_context::createPartnerContext(config);
    trace.push_back(Json{{"created",mate.get()!=
      static_cast<void*>(&config)},{"abortSame",mate->abortController==abort},
      {"isInProcess",mate->isInProcess}});
    auto sub=std::make_shared<bat_context::BatContext>();
    sub->batId="sub";sub->batType="subagent";
    sub->parentFlightId="leader";
    auto partnerBat=std::make_shared<bat_context::BatContext>();
    partnerBat->batId="worker@team";
    partnerBat->batType="teammate";
    partnerBat->batName="worker";partnerBat->teamName="team";
    const auto returned=bat_context::runWithBatContext(sub,[&]{
      trace.push_back(state(swarms));
      const auto inner=bat_context::runWithPartnerContext(mate,[&]{
        trace.push_back(state(swarms));
        return bat_context::runWithBatContext(partnerBat,[&]{
          trace.push_back(state(swarms));return std::string("nested");});
      });
      trace.push_back(Json{{"returned",inner}});trace.push_back(state(swarms));
      return std::string("outer");
    });
    trace.push_back(Json{{"returned",returned}});trace.push_back(state(swarms));
    try{bat_context::runWithPartnerContext(mate,[&]{
      trace.push_back(state(swarms));throw std::runtime_error("stop");});}
    catch(const std::exception& error){trace.push_back(Json{{"caught",error.what()}});}
    trace.push_back(state(swarms));
    std::cout<<value::stringify(trace)<<'\n';
  }
}
