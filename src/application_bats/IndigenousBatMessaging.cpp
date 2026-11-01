#include "application_bats/IndigenousBatApplication.hpp"
#include "application/Async.hpp"
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():"";}
template<class T>std::future<T> ready(T value){
  std::promise<T> result;auto future=result.get_future();result.set_value(std::move(value));return future;
}
Json parse(const Json& input){
  if(!input.is_object())throw std::invalid_argument("SendMessage input must be an object");
  for(auto it=input.begin();it!=input.end();++it){
    if(it.key()!="to"&&it.key()!="message"&&it.key()!="summary")
      throw std::invalid_argument("Unrecognized SendMessage input field: "+it.key());
    if(!it.value().is_string())throw std::invalid_argument("SendMessage "+it.key()+" must be a string");
  }
  if(!field(input,"to").is_string()||!field(input,"message").is_string())
    throw std::invalid_argument("SendMessage requires to and message");
  return input;
}
}
gadgets::Gadget IndigenousBatApplication::makeSendMessageGadget(){
  const auto weak=weak_from_this();
  gadgets::Gadget gadget;
  gadget.name="SendMessage";gadget.searchHint="send follow-up instructions to an existing child agent";
  gadget.maxResultSizeChars=100000;gadget.shouldDefer=false;
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"properties",{{"to",{{"type","string"},{"description","Existing child agent name or ID"}}},
      {"message",{{"type","string"},{"description","Follow-up instructions"}}},
      {"summary",{{"type","string"},{"description","Short summary of the message"}}}}},
    {"required",Json::array({"to","message"})}};
  gadget.parseInput=parse;
  gadget.description=[](const Json&,const Json&){return ready(std::string("Send a message to another agent"));};
  gadget.poetry=[](const gadgets::PoetryContext&){return ready(std::string(
    "Send follow-up instructions to an ordinary child agent by its Agent name or returned agentId. "
    "Include a short summary. Running agents receive the message at the next tool round. "
    "Completed or stopped agents resume their recorded conversation in the background. "
    "Use Agent to start new independent work. Team broadcasts, structured team protocols, "
    "and cross-session targets require their separate runtime providers."));};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.validateInput=[](const Json& input,const Json&){
    gadgets::ValidationResult result;
    if(value::trim(string(field(input,"summary"))).empty()){
      result.result=false;result.message="summary is required when message is a string";result.errorCode=9;
    }
    return ready(result);
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    return Json{{"tool_use_id",id},{"type","tool_result"},
      {"content",Json::array({Json{{"type","text"},{"text",value::stringify(data)}}})}};
  };
  gadget.renderGadgetUseMessage=[](const Json& input,const Json&){
    return Json(string(field(input,"summary"))+" → "+string(field(input,"to")));
  };
  gadget.renderGadgetResultMessage=[](const Json& data,const Json&,const Json&){return field(data,"message");};
  gadget.renderGadgetUseErrorMessage=[](const Json& value,const Json&){return value;};
  gadget.renderGadgetUseRejectedMessage=gadget.renderGadgetUseErrorMessage;
  gadget.call=[weak](const Json& input,const gadgets::GadgetCallContext& context,
      const IndigenousQueryBridge::CanUseGadget& canUse,const Json&,
      const std::function<void(const gadgets::GadgetProgress&)>&){
    std::promise<gadgets::GadgetResult> output;auto future=output.get_future();
    auto self=weak.lock();
    try{
      if(!self)throw std::runtime_error("Native Agent owner is unavailable");
      const auto to=string(field(input,"to"));
      const auto message=string(field(input,"message"));
      const auto state=self->appState();
      const auto id=self->resolveMissionId(to);
      Json data;
      if(!id)data={{"success",false},{"message","No ordinary child agent found: "+to}};
      else{
        const auto mission=value::property(field(state,"tasks"),*id);
        const auto status=string(field(mission,"status"));
        std::optional<bats::BatContext> parent;
        if(context.batContext)parent=*context.batContext;
        application::awaitOnOwner<event_loop::Unit>(self->inputs_.owner.dispatcher(),
          [self,id=*id,message,parent,canUse]{return self->submitToMissionWithContext(id,message,parent,canUse,false);});
        const auto report=status=="running"?
          "Message queued for delivery to "+to+" at its next tool round.":
          mission.is_null()?"Agent \""+to+"\" had no active task; resumed from transcript in the background with your message. You'll be notified when it finishes. Output: "+self->registration_.getMissionOutputPath(*id):
          "Agent \""+to+"\" was stopped ("+status+"); resumed it in the background with your message. You'll be notified when it finishes. Output: "+self->registration_.getMissionOutputPath(*id);
        data={{"success",true},{"message",report}};
      }
      gadgets::GadgetResult result;result.data=std::move(data);output.set_value(std::move(result));
    }catch(const std::exception& error){
      gadgets::GadgetResult result;result.data={{"success",false},{"message",error.what()}};output.set_value(std::move(result));
    }catch(...){output.set_exception(std::current_exception());}
    return future;
  };
  return gadgets::buildGadget(std::move(gadget));
}
} // namespace batbox::application_bats
