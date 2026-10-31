#include "sdk/EventQueue.hpp"
#include "flights/Lifecycle.hpp"
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
using namespace batbox;
Json run(const Json& item){
 bool active=item.value("active",true),emit=item.value("emit",false);int serial=0;std::string flight="s1";
 Json calls=Json::array(),results=Json::array();std::map<std::string,value::RecordRef> events;
 std::vector<std::vector<value::RecordRef>> drains;value::ValueRef lastDetails;value::RecordRef metadata;
 sdk::QueueServices q;
 q.getIsNonInteractiveFlight=[&]{calls.push_back(Json::array({"noninteractive",active}));return active;};
 q.randomUUID=[&]{calls.push_back(Json::array({"uuid"}));if(item.value("uuidError",false))throw std::runtime_error("uuid failed");return "u"+std::to_string(++serial);};
 q.getFlightId=[&]{calls.push_back(Json::array({"session"}));return flight;};sdk::EventQueue queue(q);
 flights::Lifecycle lifecycle({[&]{return emit;},[&](value::RecordRef e){queue.enqueueSdkEvent(std::move(e));}});
 const auto install=[&](const std::string& which){
  if(which=="state")lifecycle.setFlightStateChangedListener([&](const std::string& s,value::ValueRef d){
   calls.push_back(Json::array({"state",s,d?*d:Json(),lifecycle.getFlightState()}));
   if(item.value("mutateDetails",false)&&d)(*d)["note"]="changed";
   if(item.value("stateError",false))throw std::runtime_error("state failed");
   if(item.value("reentrant",false)&&s=="requires_action")lifecycle.notifyFlightStateChanged("running");
  });
  else if(which=="metadata")lifecycle.setFlightMetadataChangedListener([&](value::RecordRef m){
   calls.push_back(Json::array({"metadata",m->snapshot(),m->get("pending_action")==lastDetails}));metadata=m;
   if(item.value("metadataError",false))throw std::runtime_error("metadata failed");
  });
  else if(which=="permission")lifecycle.setPermissionModeChangedListener([&](const std::string& mode){calls.push_back(Json::array({"permission",mode}));});
 };
 for(const auto& step:item.at("steps")){try{
  Json result;const auto op=step.at("op").get<std::string>();
  if(op=="install")install(step.at("value"));
  else if(op=="clear"){
   if(step["value"]=="state")lifecycle.setFlightStateChangedListener({});
   else if(step["value"]=="metadata")lifecycle.setFlightMetadataChangedListener({});
   else lifecycle.setPermissionModeChangedListener({});
  }else if(op=="state"){
   lastDetails=step.contains("details")?std::make_shared<Json>(step["details"]):nullptr;
   lifecycle.notifyFlightStateChanged(step["value"],lastDetails);result=lifecycle.getFlightState();
  }else if(op=="metadata"){
   auto m=std::make_shared<value::SharedRecord>();m->set("model","local");lifecycle.notifyFlightMetadataChanged(m);result=metadata==m;
  }else if(op=="permission")lifecycle.notifyPermissionModeChanged(step["value"]);
  else if(op=="enqueue"){
   auto event=std::make_shared<value::SharedRecord>(value::SharedRecord::fromJson(step["value"]));events[step["id"]]=event;queue.enqueueSdkEvent(event);
  }else if(op=="many")for(int i=0;i<step["count"].get<int>();++i){auto e=std::make_shared<value::SharedRecord>();e->set("index",i);queue.enqueueSdkEvent(e);}
  else if(op=="mutate")events.at(step["id"])->set(step["key"],step["value"]);
  else if(op=="mutateNested")(*events.at(step["id"])->get(step["key"]))[step["field"].get<std::string>()]=step["value"];
  else if(op=="drain"){
   auto list=queue.drainSdkEvents();drains.push_back(list);result=Json::array();
   for(const auto& e:list)result.push_back(Json{{"snapshot",e->snapshot()},{"keys",e->keys()}});
  }else if(op=="drained"){result=Json::array();for(const auto& e:drains.at(step["index"].get<std::size_t>()))result.push_back(e->snapshot());}
  else if(op=="terminal"){
   std::optional<sdk::TerminationOptions> opts;
   if(step.contains("opts")&&!step["opts"].is_null()){
    opts.emplace();const auto& o=step["opts"];
    if(o.contains("toolUseId")&&!o["toolUseId"].is_null())opts->gadgetUseId=o["toolUseId"];
    if(o.contains("summary")&&!o["summary"].is_null())opts->summary=o["summary"];
    if(o.contains("outputFile")&&!o["outputFile"].is_null())opts->outputFile=o["outputFile"];
    if(o.contains("usage"))opts->usage=std::make_shared<Json>(o["usage"]);
   }queue.emitMissionTerminatedSdk(step["id"],step["status"],opts);
  }else if(op=="active")active=step["value"];
  else if(op=="session")flight=step["value"];
  else if(op=="emit")emit=step["value"];
  else throw std::runtime_error("unknown events step");
  results.push_back(result);
 }catch(const std::exception& e){results.push_back(Json{{"error",e.what()}});}}
 return Json{{"results",results},{"calls",calls}};
}
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();for(const auto& item:input)out.push_back(run(item));std::cout<<value::stringify(out)<<'\n';}
