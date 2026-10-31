#include "diagnostics/DiagLogs.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using batbox::Json;
  const auto input=Json::parse(std::cin);
  std::string file=input.value("file",std::string{});
  bool failOnce=false,failMkdir=false;
  Json calls=Json::array(),writes=Json::array(),out=Json::array();
  batbox::diagnostics::Services services;
  services.diagnosticFile=[&]()->std::optional<std::string>{
    return file.empty()?std::nullopt:std::optional(file);};
  services.timestamp=[](){return std::string("2026-10-28T12:00:00.000Z");};
  services.nowMs=[](){return 0.0;};
  services.appendFileSync=[&](const std::string& path,const std::string& line){
    calls.push_back(Json::array({"append",path,line}));
    if(failOnce){failOnce=false;throw std::runtime_error("append failed");}
    writes.push_back(Json::array({path,line}));
  };
  services.mkdirSync=[&](const std::string& path){
    calls.push_back(Json::array({"mkdir",path}));
    if(failMkdir)throw std::runtime_error("mkdir failed");
  };
  batbox::diagnostics::Logger logger(services);
  for(const auto& action:input["actions"]){
    const auto kind=action["kind"].get<std::string>();
    if(kind=="path")file=action["value"].get<std::string>();
    if(kind=="failOnce")failOnce=true;
    if(kind=="failMkdir")failMkdir=action["value"].get<bool>();
    if(kind=="log")logger.logForDiagnosticsNoPII(
      action["level"].get<std::string>(),action["event"].get<std::string>(),
      action.value("data",Json::object()));
    out.push_back({{"calls",calls},{"writes",writes}});
  }
  std::cout<<out.dump()<<'\n';
}
