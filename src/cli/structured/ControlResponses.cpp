#include "cli/structured/ControlResponses.hpp"
#include <stdexcept>

namespace batbox::cli::structured {
ControlResponses::ControlResponses(std::function<void(const Json&)> write):
  write_(std::move(write)){
  if(!write_)throw std::invalid_argument("Structured IO writer required");
}
void ControlResponses::addPendingRequest(const std::string& id,
                                         PendingRequest request){
  if(!request.resolve||!request.reject)
    throw std::invalid_argument("Pending request callbacks required");
  if(const auto found=positions_.find(id);found!=positions_.end())
    found->second->value=std::move(request);
  else{
    pending_.push_back({id,std::move(request)});
    positions_.emplace(id,std::prev(pending_.end()));
  }
}
void ControlResponses::trackResolvedGadgetUseId(const Json& request){
  const auto& body=value::property(request,"request");
  if(value::property(body,"subtype")!="can_use_tool")return;
  const auto& id=value::property(body,"tool_use_id");
  if(!id.is_string())throw std::invalid_argument("Typed tool use ID required");
  resolved_.insert(id.get<std::string>());
  if(resolved_.size()>1000)resolved_.erase(*resolved_.begin());
}
Json ControlResponses::getPendingPermissionRequests()const{
  Json output=Json::array();
  for(const auto& entry:pending_)
    if(value::property(value::property(entry.value.request,"request"),"subtype")==
       "can_use_tool")output.push_back(entry.value.request);
  return output;
}
void ControlResponses::injectControlResponse(const Json& response){
  const auto& envelope=value::property(response,"response");
  const auto& id=value::property(envelope,"request_id");
  if(!value::truthy(id))return;
  if(!id.is_string())return; // Typed SDK request IDs are strings.
  const auto requestId=id.get<std::string>();
  auto pending=takePendingRequest(requestId);
  if(!pending)return;
  write_(Json{{"type","control_cancel_request"},{"request_id",requestId}});
  if(value::property(envelope,"subtype")=="error"){
    const auto& error=value::property(envelope,"error");
    pending->reject(error.is_string()?error.get<std::string>():
      value::stringify(error));
  }else{
    const auto& result=value::property(envelope,"response");
    if(pending->parseResponse){
      try{pending->resolve(pending->parseResponse(result));}
      catch(const std::exception& error){pending->reject(error.what());}
    }else pending->resolve(Json::object());
  }
}
bool ControlResponses::hasResolvedGadgetUseId(const std::string& id)const{
  return resolved_.contains(id);
}
std::size_t ControlResponses::resolvedGadgetUseCount()const{
  return resolved_.size();
}
bool ControlResponses::hasPendingRequest(const std::string& id)const{
  return positions_.contains(id);
}
std::optional<PendingRequest> ControlResponses::takePendingRequest(
    const std::string& id){
  const auto found=positions_.find(id);
  if(found==positions_.end())return {};
  trackResolvedGadgetUseId(found->second->value.request);
  PendingRequest request=std::move(found->second->value);
  pending_.erase(found->second);
  positions_.erase(found);
  return request;
}
void ControlResponses::abortPendingRequest(const std::string& id,
                                            std::exception_ptr error){
  const auto found=positions_.find(id);
  if(found==positions_.end())return;
  trackResolvedGadgetUseId(found->second->value.request);
  if(found->second->value.rejectException)
    found->second->value.rejectException(error);
  else{
    try{std::rethrow_exception(error);}
    catch(const std::exception& failure){
      found->second->value.reject(failure.what());
    }
  }
}
void ControlResponses::erasePendingRequest(const std::string& id){
  const auto found=positions_.find(id);
  if(found==positions_.end())return;
  pending_.erase(found->second);
  positions_.erase(found);
}
void ControlResponses::rejectAllPending(const std::string& error){
  for(auto& entry:pending_)entry.value.reject(error);
}
}
