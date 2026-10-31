#include "bats/BatMissionNotification.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json run(const Json& item){
 Json trace=Json::array();
 Json state={{"tasks",{{"task",{{"type","local_agent"},
  {"notified",field(item,"notified")}}}}}};
 ba::BatMissionNotificationServices services;
 services.setAppState=[&](const std::function<Json(const Json&)>& updater){
  trace.push_back(Json::array({"setState"}));state=updater(state);};
 services.abortSpeculation=[&](const ba::RootSetAppState&){
  trace.push_back(Json::array({"abortSpeculation"}));};
 services.getMissionOutputPath=[&](const std::string& id){
  trace.push_back(Json::array({"outputPath",id}));return "/tmp/"+id+".txt";};
 services.enqueuePendingNotification=[&](const Json& notice){
  trace.push_back(Json::array({"enqueue",notice}));};
 const Json& repeat=field(item,"repeat");
 const int count=repeat.is_number_integer()?repeat.get<int>():1;
 for(int i=0;i<count;++i)
  ba::enqueueBatNotification(field(item,"args"),services);
 return Json{{"trace",trace},{"state",state}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
