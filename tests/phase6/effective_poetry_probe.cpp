#include "poetryEntries/EffectiveSystemPoetry.hpp"
#include <iostream>
using namespace batbox;
int main(){
 for(std::string line;std::getline(std::cin,line);){
  const auto p=Json::parse(line);Json trace=Json::array(),result=nullptr,error=nullptr;
  poetryEntries::EffectivePoetryParams input;poetryEntries::EffectivePoetryServices s;
  input.gadgetUseContext.data={{"options",{{"model","m"}}},{"secret","hidden"}};
  input.defaultSystemPoetry=Json::array({"base","environment"});
  input.customSystemPoetry=value::property(p,"custom");input.appendSystemPoetry=value::property(p,"append");
  input.overrideSystemPoetry=value::property(p,"override");
  s.feature=[&](const std::string& name){const auto f=value::property(p,"features");
    return f.is_array()&&std::find(f.begin(),f.end(),name)!=f.end();};
  s.coordinatorModeEnabled=[&]{return p.value("coordinator",false);};
  s.isProactiveActive=[&]{return p.value("active",false);};
  s.isBuiltInBat=[](const Json& d){return d.value("builtIn",false);};
  s.getCoordinatorSystemPoetry=[&]{trace.push_back("coordinator");return "lead";};
  if(p.value("agent",false)){
    input.mainThreadBat=std::make_shared<poetryEntries::MainThreadPoetryBat>();
    input.mainThreadBat->definition={{"builtIn",p.value("builtIn",false)}};
    input.mainThreadBat->getSystemPoetry=[&](const execution::Context* context){
      trace.push_back(Json::array({"agent",context?Json{{"toolUseContext",context->data}}:Json(nullptr)}));
      if(p.value("error",false))throw std::runtime_error("prompt failed");
      return value::property(p,"agentPrompt");
    };
  }
  try{result=poetryEntries::buildEffectiveSystemPoetry(input,s);}catch(const std::exception& e){error=e.what();}
  std::cout<<Json{{"trace",trace},{"result",result},{"error",error}}.dump()<<'\n';
 }
}
