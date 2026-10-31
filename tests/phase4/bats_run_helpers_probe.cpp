#include "bats/RunBatHelpers.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& value,const char* key){return value::property(value,key);}
static Json run(const Json& item){
 Json trace=Json::array(),output=nullptr,error=nullptr;
 const auto getState=[&]{trace.push_back(Json::array({"getAppState"}));return field(item,"state");};
 const auto feature=[&](const std::string& name){trace.push_back(Json::array({"feature",name}));
  return field(item,"transcriptClassifier")==true;};
 std::optional<bool> poetryEntries;
 if(field(item,"canShowPermissionPrompts").is_boolean())
  poetryEntries=field(item,"canShowPermissionPrompts").get<bool>();
 std::optional<Json> allowed;
 if(item.contains("allowedTools"))allowed=field(item,"allowedTools");
 try{output=ba::createBatGetAppState(getState,field(item,"agentDefinition"),
  field(item,"isAsync")==true,poetryEntries,allowed,feature)();}
 catch(const std::exception& caught){error=caught.what();}
 Json recordable=Json::array();
 const Json& messages=field(item,"messages");
 for(const Json& message:messages)recordable.push_back(ba::isRecordableBatMessage(message));
 return Json{{"trace",trace},{"output",output},{"error",error},
  {"filtered",ba::filterIncompleteGadgetCalls(messages)},{"recordable",recordable}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
