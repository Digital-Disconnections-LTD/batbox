#include "application_hooks/Provider.hpp"
#include "model/QueryModel.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown function hook error";}
  return {};
}
}
void Provider::bindFunctionHooks(){
  hooks::FunctionHookServices services;
  services.logError=[log=inputs_.log](std::exception_ptr error){
    log("Function hook execution failed: "+errorText(error));};
  services.createAttachmentMessage=inputs_.createAttachmentMessage;
  services.isErrorInstance=[](std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception&){return true;}
    catch(...){return false;}
    return false;};
  services.isAbortError=[](std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const model::UserAbortError&){return true;}
    catch(...){return false;}
    return false;};
  services.errorMessage=errorText;
  functionHost_=std::make_shared<hooks::FunctionHookHost>(owner_,
    std::move(services));
}
std::string Provider::addFunctionHook(const std::string& flightId,
    const std::string& event,const std::string& matcher,
    std::function<hooks::FunctionHookReply(const Json&,std::stop_token)> callback,
    const std::string& errorMessage,std::optional<double> timeout){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Function hook registration requires owner loop");
  if(!indigenousFlightStore_)throw std::logic_error(
    "Function hook session store is unavailable");
  return indigenousFlightStore_->addFunctionHook(flightId,event,matcher,
    std::move(callback),errorMessage,timeout);
}
void Provider::removeFunctionHook(const std::string& flightId,
    const std::string& event,const std::string& id){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Function hook removal requires owner loop");
  if(!indigenousFlightStore_)throw std::logic_error(
    "Function hook session store is unavailable");
  indigenousFlightStore_->removeFunctionHook(flightId,event,id);
}
} // namespace batbox::application_hooks
