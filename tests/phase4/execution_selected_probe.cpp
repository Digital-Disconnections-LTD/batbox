#include "execution/GadgetExecution.hpp"
#include <algorithm>
#include <future>
#include <iostream>
using namespace batbox;
template<class T>std::future<T>ready(T value){std::promise<T> p;p.set_value(std::move(value));return p.get_future();}
namespace {
const Json& field(const Json& object,const char* name){return value::property(object,name);}
bool flag(const Json& item,const char* name){return value::truthy(field(item,name));}
}
int main(){std::string line;while(std::getline(std::cin,line))try{
  const Json item=value::parse(line);
  Json calledInput,results=Json::array();
  gadgets::Gadget gadget;gadget.name="FileRead";
  gadget.parseInput=[](const Json& input){return input;};
  gadget.backfillObservableInput=[&](Json& input){
    if(flag(item,"backfillPath"))input["file_path"]="/absolute/file";
    if(flag(item,"backfillDerived"))input["derived"]="derived";
  };
  gadget.call=[&](const Json& input,const gadgets::GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const gadgets::GadgetProgress&)>&){
    calledInput=input;gadgets::GadgetResult result;result.data=Json{{"text","done"}};
    return ready(result);
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    return Json{{"type","tool_result"},{"content",data.at("text")},{"tool_use_id",id}};
  };
  execution::Context context;context.gadgets={&gadget};context.data={{"messages",Json::array()}};
  execution::Services services;
  services.createUserMessage=[](const Json& args){return Json{{"type","user"},{"args",args}};};
  services.preGadgetUseHooks=[&](const gadgets::Gadget&,const Json&,const Json&,
    const execution::Context&,const std::function<void(const Json&)>& yield){
    const Json& hooks=field(item,"hooks");
    if(hooks.is_array())for(const Json& hook:hooks)yield(hook);
  };
  services.resolvePermissionDecision=[&](const Json&,const gadgets::Gadget&,const Json& input,
    const execution::Context&,const Json&,const std::string&){
    const std::string mode=field(item,"permissionInput").is_string()?
      field(item,"permissionInput").get<std::string>():std::string{};
    Json updated=input;
    if(mode=="fresh_changed")updated["other"]="changed";
    if(mode=="original")updated=field(item,"input");
    const bool hasHookUpdate=field(item,"hooks").is_array()&&
      std::any_of(field(item,"hooks").begin(),field(item,"hooks").end(),[](const Json& event){
        return field(event,"type")=="hookUpdatedInput";
      });
    return Json{{"decision",{{"behavior","allow"}}},{"input",updated},
      {"inputIsBackfilledClone",mode!="fresh_equal"&&mode!="fresh_changed"&&
        mode!="original"&&!hasHookUpdate}};
  };
  services.postGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
    const execution::Context&,const std::function<void(const Json&)>&){};
  services.processPreMappedGadgetResultBlock=[](const Json& block,const gadgets::Gadget&){return block;};
  services.canUseGadget=[](const Json&){return ready(Json{{"behavior","allow"}});};
  services.addToGadgetDuration=[](double){};
  services.nowMillis=[]{return std::int64_t{1};};
  const Json assistant={{"uuid","parent"},{"message",{{"id","message"}}}};
  const Json block={{"id","tool"},{"name","FileRead"},{"input",field(item,"input")}};
  execution::runGadgetUse(block,assistant,context,services,[&](const execution::Update& update){
    if(update.message)results.push_back(Json{{"message",*update.message}});
  });
  std::cout<<value::stringify(Json{{"calledInput",calledInput},{"results",results}})<<'\n';
}catch(const std::exception& error){std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}}
