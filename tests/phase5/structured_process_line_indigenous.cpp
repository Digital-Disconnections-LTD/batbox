#include "cli/structured/IndigenousProcessLine.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  cli::structured::ControlResponses control([](const Json&){});
  cli::structured::ProcessLineServices services;
  cli::structured::bindIndigenousProcessLineOS(services);
  services.logForDebugging=[](const std::string&,const Json&){};
  const std::string key="BATBOX_STRUCTURED_INPUT_NATIVE_TEST";
  const auto line=value::stringify(Json{{"type","update_environment_variables"},
    {"variables",Json{{key,"refreshed"}}}});
  auto pending=cli::structured::processLine(loop,line,control,services);
  bool settled=false;
  pending.then([&](const std::optional<Json>* result,const auto& error){
    if(error)std::rethrow_exception(error);
    if(!result||*result)throw std::runtime_error("Expected no input message");
    settled=true;
  });
  loop.runReady();
  const auto value=std::getenv(key.c_str());
  if(!settled||!value||std::string(value)!="refreshed")
    throw std::runtime_error("Environment update did not reach process");
  ::unsetenv(key.c_str());
}
