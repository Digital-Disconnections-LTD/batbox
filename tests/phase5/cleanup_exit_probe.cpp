#include "cleanup/Exit.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace batbox;
namespace {
struct Case {std::string name;bool test=false,exitThrow=false,drainThrow=false;};
Json run(const Case& item){
  Json logs=Json::array();cleanup::ExitServices services;
  services.clearFailsafe=[&]{logs.push_back("clear");};
  services.drainStdin=[&]{logs.push_back("drain");
    if(item.drainThrow)throw std::runtime_error("drain");};
  services.processExit=[&](int code){logs.push_back("exit:"+std::to_string(code));
    if(item.exitThrow)throw std::runtime_error("exit failed");};
  services.killSelf=[&]{logs.push_back("kill:123:SIGKILL");};
  services.isTestMode=[&]{return item.test;};
  std::string value="returned";
  try{cleanup::forceExit(9,services);}
  catch(const std::exception& error){value="ERROR:"+std::string(error.what());}
  return Json{{"name",item.name},{"value",value},{"logs",logs}};
}
}
int main(){
  const std::vector<Case> cases={{"test-return",true,false,false},
    {"test-throw",true,true,false},{"prod-return",false,false,false},
    {"prod-throw",false,true,true}};
  Json out=Json::array();for(const auto& item:cases)out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
